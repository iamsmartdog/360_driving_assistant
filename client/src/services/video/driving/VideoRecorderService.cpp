#include "VideoRecorderService.h"
#include "services/video/common/DetectionEngine.h"
#include "services/video/common/VideoFrameProvider.h"
#include "services/config/AppConfig.h"
#include <QDir>
#include <QDebug>
#include <QFileInfo>
#include <QCoreApplication>

VideoRecorderService::VideoRecorderService(VideoFrameProvider *provider, QObject *parent)
    : QObject(parent)
    , m_detectionEngine(new DetectionEngine(this))
    , m_isRecording(false)
    , m_isDetecting(true)
    , m_isTrafficLightDetecting(true)
    , m_cameraOpened(false)
    , m_recordDuration(0)
    , m_writtenFrames(0)
    , m_videoDir(AppConfig::instance().videoDir())
    , m_captureTimer(new QTimer(this))
    , m_durationTimer(new QTimer(this))
    , m_cameraFps(0)
    , m_cameraWidth(0)
    , m_cameraHeight(0)
    , m_trafficLightState("unknown")
    , m_isAutoScreenshotEnabled(false)
    , m_autoScreenshotIntervalSec(15)
    , m_screenshotTimer(new QTimer(this))
    , m_frameProvider(provider)
    , m_frameCounter(0)
{
    // 采集定时器：默认30fps
    m_captureTimer->setInterval(33);
    connect(m_captureTimer, &QTimer::timeout, this, &VideoRecorderService::onCaptureTimer);

    // 录制时长计时器 - 每秒基于实际写入帧数更新时长（而非墙钟时间）
    m_durationTimer->setInterval(1000);
    connect(m_durationTimer, &QTimer::timeout, this, [this]() {
        // 用实际帧数计算真实时长，避免YOLO耗时时长不准
        double fps = m_cameraFps > 0 ? m_cameraFps : 30;
        int actualSec = static_cast<int>(m_writtenFrames / fps);
        if (actualSec != m_recordDuration) {
            m_recordDuration = actualSec;
            emit recordDurationChanged();
        }
    });

    // 自动截图定时器
    m_screenshotTimer->setInterval(m_autoScreenshotIntervalSec * 1000);
    connect(m_screenshotTimer, &QTimer::timeout, this, &VideoRecorderService::onScreenshotTimer);

    // 确保视频目录存在
    QDir dir(m_videoDir);
    if (!dir.exists()) {
        dir.mkpath(".");
    }

    // 确保截图子目录存在
    QDir screenshotDir(m_videoDir + "/screenshots");
    if (!screenshotDir.exists()) {
        screenshotDir.mkpath(".");
    }

    if (m_detectionEngine->isLoaded()) {
        qDebug() << "检测引擎(YOLOv8n)初始化成功";
    } else {
        qWarning() << "检测引擎(YOLOv8n)初始化失败，检测功能不可用";
    }
}

VideoRecorderService::~VideoRecorderService()
{
    stopRecording();
    closeCamera();
}

bool VideoRecorderService::isRecording() const { return m_isRecording; }
bool VideoRecorderService::isDetecting() const { return m_isDetecting; }
bool VideoRecorderService::isCameraOpen() const { return m_cameraOpened; }
QString VideoRecorderService::recordFileName() const { return m_recordFileName; }
int VideoRecorderService::recordDuration() const { return m_recordDuration; }
QString VideoRecorderService::videoDir() const { return m_videoDir; }
QVariantList VideoRecorderService::detectedVehicles() const { return m_detectedVehicles; }
QString VideoRecorderService::trafficLightState() const { return m_trafficLightState; }
bool VideoRecorderService::isTrafficLightDetecting() const { return m_isTrafficLightDetecting; }
int VideoRecorderService::cameraFps() const { return m_cameraFps; }
int VideoRecorderService::cameraWidth() const { return m_cameraWidth; }
int VideoRecorderService::cameraHeight() const { return m_cameraHeight; }
bool VideoRecorderService::isAutoScreenshotEnabled() const { return m_isAutoScreenshotEnabled; }
int VideoRecorderService::autoScreenshotIntervalSec() const { return m_autoScreenshotIntervalSec; }
int VideoRecorderService::frameCounter() const { return m_frameCounter; }

void VideoRecorderService::setDetecting(bool detect)
{
    if (m_isDetecting != detect) {
        m_isDetecting = detect;
        emit detectingChanged();
        if (!detect) {
            m_detectedVehicles.clear();
            emit detectedVehiclesChanged();
        }
    }
}

void VideoRecorderService::setTrafficLightDetecting(bool detect)
{
    if (m_isTrafficLightDetecting != detect) {
        m_isTrafficLightDetecting = detect;
        emit trafficLightDetectingChanged();
        if (!detect) {
            m_trafficLightState = "unknown";
            m_detectedTrafficLights.clear();
            emit trafficLightStateChanged();
        }
    }
}

void VideoRecorderService::setAutoScreenshotEnabled(bool enabled)
{
    if (m_isAutoScreenshotEnabled != enabled) {
        m_isAutoScreenshotEnabled = enabled;
        emit autoScreenshotEnabledChanged();

        if (enabled && m_cameraOpened) {
            m_screenshotTimer->start();
        } else {
            m_screenshotTimer->stop();
        }
    }
}

void VideoRecorderService::setAutoScreenshotIntervalSec(int sec)
{
    if (sec < 5) sec = 5;
    if (sec > 120) sec = 120;
    if (m_autoScreenshotIntervalSec != sec) {
        m_autoScreenshotIntervalSec = sec;
        m_screenshotTimer->setInterval(sec * 1000);
        emit autoScreenshotIntervalChanged();
    }
}

void VideoRecorderService::setVideoDir(const QString &dir)
{
    if (m_videoDir != dir) {
        m_videoDir = dir;
        QDir d(dir);
        if (!d.exists()) d.mkpath(".");
        QDir screenshotDir(dir + "/screenshots");
        if (!screenshotDir.exists()) screenshotDir.mkpath(".");
        emit videoDirChanged();
    }
}

bool VideoRecorderService::openCamera(int deviceId)
{
    if (m_cameraOpened) {
        qWarning() << "摄像头已打开，请先关闭";
        return true;
    }

    m_capture.open(deviceId);
    if (!m_capture.isOpened()) {
        qWarning() << "无法打开摄像头，设备ID:" << deviceId;
        m_cameraOpened = false;
        emit cameraStateChanged();
        return false;
    }

    m_cameraFps = static_cast<int>(m_capture.get(cv::CAP_PROP_FPS));
    m_cameraWidth = static_cast<int>(m_capture.get(cv::CAP_PROP_FRAME_WIDTH));
    m_cameraHeight = static_cast<int>(m_capture.get(cv::CAP_PROP_FRAME_HEIGHT));

    if (m_cameraFps <= 0) m_cameraFps = 30;

    qDebug() << "摄像头已打开:" << m_cameraWidth << "x" << m_cameraHeight
             << "@" << m_cameraFps << "fps";

    m_cameraOpened = true;
    m_captureTimer->start();

    if (m_isAutoScreenshotEnabled) {
        m_screenshotTimer->start();
    }

    emit cameraStateChanged();
    emit cameraInfoChanged();
    return true;
}

void VideoRecorderService::closeCamera()
{
    if (!m_cameraOpened) return;

    m_captureTimer->stop();
    m_screenshotTimer->stop();

    if (m_isRecording) {
        stopRecording();
    }

    if (m_capture.isOpened()) {
        m_capture.release();
    }

    m_cameraOpened = false;
    m_cameraFps = 0;
    m_cameraWidth = 0;
    m_cameraHeight = 0;
    m_lastFrame.release();

    emit cameraStateChanged();
    emit cameraInfoChanged();

    qDebug() << "摄像头已关闭";
}

bool VideoRecorderService::startRecording()
{
    if (m_isRecording) return false;

    if (!m_cameraOpened || !m_capture.isOpened()) {
        qWarning() << "摄像头未打开，无法录制";
        return false;
    }

    QDateTime now = QDateTime::currentDateTime();
    QString fileName = "行车记录_" + now.toString("yyyyMMdd_HHmmss") + ".avi";
    QString filePath = m_videoDir + "/" + fileName;

    double fps = m_capture.get(cv::CAP_PROP_FPS);
    if (fps <= 0) fps = 30.0;

    int frameWidth = static_cast<int>(m_capture.get(cv::CAP_PROP_FRAME_WIDTH));
    int frameHeight = static_cast<int>(m_capture.get(cv::CAP_PROP_FRAME_HEIGHT));

    int fourcc = cv::VideoWriter::fourcc('M', 'J', 'P', 'G');
    m_writer.open(filePath.toStdString(), fourcc, fps,
                  cv::Size(frameWidth, frameHeight), true);

    if (!m_writer.isOpened()) {
        qWarning() << "无法创建视频文件:" << filePath;
        return false;
    }

    m_recordFileName = fileName;
    m_recordDuration = 0;
    m_writtenFrames = 0;
    m_isRecording = true;

    emit recordFileNameChanged();
    emit recordingStateChanged();
    emit recordDurationChanged();

    m_durationTimer->start();

    qDebug() << "开始录制:" << filePath;
    return true;
}

void VideoRecorderService::stopRecording()
{
    if (!m_isRecording) return;

    m_isRecording = false;
    m_durationTimer->stop();

    QString filePath = m_videoDir + "/" + m_recordFileName;

    // 用实际写入的帧数计算真实视频时长（而非墙钟时间）
    // 因为YOLO检测耗时会降低实际帧率，导致实际帧数 < 墙钟时间*目标FPS
    double fps = m_cameraFps > 0 ? m_cameraFps : 30;
    int actualDurationSec = static_cast<int>(m_writtenFrames / fps);

    QFileInfo fi(filePath);
    int fileSizeMB = fi.exists() ? static_cast<int>(fi.size() / (1024 * 1024)) : 0;

    if (m_writer.isOpened()) {
        m_writer.release();
    }

    // 更新recordDuration为真实视频时长
    m_recordDuration = actualDurationSec;

    emit recordingStateChanged();
    emit recordDurationChanged();
    emit recordingSaved(filePath, actualDurationSec, fileSizeMB,
                        m_cameraWidth, m_cameraHeight, m_cameraFps);

    qDebug() << "停止录制，保存文件:" << filePath
             << "实际写入帧数:" << m_writtenFrames
             << "真实时长:" << actualDurationSec << "秒"
             << "(墙钟时间不同步是因为YOLO检测降低了帧率)"
             << "大小:" << fileSizeMB << "MB";
}

void VideoRecorderService::onCaptureTimer()
{
    if (m_cameraOpened && m_capture.isOpened()) {
        processFrame();
    }
}

void VideoRecorderService::processFrame()
{
    cv::Mat frame;
    if (!m_capture.read(frame) || frame.empty()) {
        qWarning() << "读取摄像头帧失败";
        return;
    }

    // ===== 使用 DetectionEngine（YOLOv8n）进行检测 =====
    // 每2帧执行一次检测（YOLOv8单次推理同时获取车辆+红绿灯，比原来分开跑更高效）
    bool needDetect = (m_isDetecting || m_isTrafficLightDetecting)
                      && m_detectionEngine->isLoaded()
                      && (m_frameCounter % 2 == 0);

    if (needDetect) {
        QVariantList vehicleResults;
        TrafficLightResult tlResult;

        // 单次推理同时获取车辆和红绿灯
        if (m_isDetecting && m_isTrafficLightDetecting) {
            m_detectionEngine->detectAll(frame, vehicleResults, tlResult);
        } else if (m_isDetecting) {
            m_detectionEngine->detectVehicles(frame, vehicleResults);
        } else if (m_isTrafficLightDetecting) {
            m_detectionEngine->detectTrafficLights(frame, tlResult);
        }

        // 更新车辆检测结果
        if (m_isDetecting && vehicleResults != m_detectedVehicles) {
            m_detectedVehicles = vehicleResults;
            emit detectedVehiclesChanged();
        }

        // 更新红绿灯检测结果
        if (m_isTrafficLightDetecting) {
            m_detectedTrafficLights = tlResult.trafficLightRects;
            if (tlResult.state != m_trafficLightState) {
                m_trafficLightState = tlResult.state;
                emit trafficLightStateChanged();

                if (tlResult.state != "unknown") {
                    qDebug() << "红绿灯检测:" << tlResult.state
                             << "检测框数:" << tlResult.trafficLightRects.size();
                }
            }
        }
    }

    // 绘制检测框
    if ((m_isDetecting || m_isTrafficLightDetecting) && m_detectionEngine->isLoaded()) {
        m_detectionEngine->drawDetections(frame, m_detectedVehicles,
                                           m_detectedTrafficLights,
                                           m_trafficLightState);
    }

    // 录制帧（已包含检测框）
    if (m_isRecording && m_writer.isOpened()) {
        m_writer.write(frame);
        m_writtenFrames++;
    }

    // 缓存最后一帧用于自动截图
    m_lastFrame = frame.clone();

    // 发送帧到QML显示
    cv::Mat rgbFrame;
    cv::cvtColor(frame, rgbFrame, cv::COLOR_BGR2RGB);
    QImage qimg(rgbFrame.data, rgbFrame.cols, rgbFrame.rows, rgbFrame.step,
                QImage::Format_RGB888);
    QImage frameCopy = qimg.copy();

    if (m_frameProvider) {
        m_frameProvider->updateFrame("live", frameCopy);
    }

    m_frameCounter++;
    emit frameCounterChanged();
}

void VideoRecorderService::onScreenshotTimer()
{
    if (m_lastFrame.empty()) return;

    bool hasDetection = !m_detectedVehicles.isEmpty() || m_trafficLightState != "unknown";
    if (!hasDetection) return;

    QDateTime now = QDateTime::currentDateTime();
    QString fileName = "截图_" + now.toString("yyyyMMdd_HHmmss") + ".png";
    QString screenshotDir = m_videoDir + "/screenshots/";
    QString filePath = screenshotDir + fileName;

    QDir dir(screenshotDir);
    if (!dir.exists()) dir.mkpath(".");

    if (cv::imwrite(filePath.toStdString(), m_lastFrame)) {
        QString detectionInfo;
        if (!m_detectedVehicles.isEmpty()) {
            detectionInfo += QString("vehicles:%1").arg(m_detectedVehicles.size());
        }
        if (m_trafficLightState != "unknown") {
            if (!detectionInfo.isEmpty()) detectionInfo += ",";
            detectionInfo += QString("traffic_light:%1").arg(m_trafficLightState);
        }

        emit screenshotSaved(filePath, detectionInfo);
        qDebug() << "自动截图保存成功:" << filePath << "检测信息:" << detectionInfo;
    } else {
        qWarning() << "自动截图保存失败:" << filePath;
    }
}

QString VideoRecorderService::takeManualScreenshot()
{
    if (m_lastFrame.empty()) {
        qWarning() << "无可用帧，无法手动截图";
        return "";
    }

    QDateTime now = QDateTime::currentDateTime();
    QString fileName = "手动截图_" + now.toString("yyyyMMdd_HHmmss") + ".png";
    QString screenshotDir = m_videoDir + "/screenshots/";
    QString filePath = screenshotDir + fileName;

    QDir dir(screenshotDir);
    if (!dir.exists()) dir.mkpath(".");

    if (cv::imwrite(filePath.toStdString(), m_lastFrame)) {
        QString detectionInfo;
        if (!m_detectedVehicles.isEmpty()) {
            detectionInfo += QString("vehicles:%1").arg(m_detectedVehicles.size());
        }
        if (m_trafficLightState != "unknown") {
            if (!detectionInfo.isEmpty()) detectionInfo += ",";
            detectionInfo += QString("traffic_light:%1").arg(m_trafficLightState);
        }

        emit screenshotSaved(filePath, detectionInfo);
        qDebug() << "手动截图保存成功:" << filePath << "检测信息:" << detectionInfo;
        return filePath;
    } else {
        qWarning() << "手动截图保存失败:" << filePath;
        return "";
    }
}
