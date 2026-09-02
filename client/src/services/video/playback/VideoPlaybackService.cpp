#include "VideoPlaybackService.h"
#include "services/video/common/DetectionEngine.h"
#include "services/video/common/VideoFrameProvider.h"
#include <QDir>
#include <QDebug>
#include <QDateTime>

VideoPlaybackService::VideoPlaybackService(VideoFrameProvider *provider, QObject *parent)
    : QObject(parent)
    , m_playTimer(new QTimer(this))
    , m_playbackSpeed(1.0)
    , m_isPlaying(false)
    , m_currentFrame(0)
    , m_totalFrames(0)
    , m_fps(30.0)
    , m_frameWidth(0)
    , m_frameHeight(0)
    , m_videoLoaded(false)
    , m_currentPositionMs(0)
    , m_totalDurationMs(0)
    , m_vehicleDetectEnabled(true)
    , m_trafficLightDetectEnabled(true)
    , m_detectionEngine(new DetectionEngine(this))
    , m_frameProvider(provider)
    , m_frameCounter(0)
{
    m_playTimer->setInterval(33);
    connect(m_playTimer, &QTimer::timeout, this, &VideoPlaybackService::onPlayTimer);

    if (m_detectionEngine->isLoaded()) {
        qDebug() << "播放服务-检测引擎(YOLOv8n)初始化成功";
    } else {
        qWarning() << "播放服务-检测引擎(YOLOv8n)初始化失败";
    }
}

VideoPlaybackService::~VideoPlaybackService()
{
    closeVideo();
}

bool VideoPlaybackService::isPlaying() const { return m_isPlaying; }
int VideoPlaybackService::currentFrame() const { return m_currentFrame; }
int VideoPlaybackService::totalFrames() const { return m_totalFrames; }
double VideoPlaybackService::fps() const { return m_fps; }
int VideoPlaybackService::frameWidth() const { return m_frameWidth; }
int VideoPlaybackService::frameHeight() const { return m_frameHeight; }
bool VideoPlaybackService::videoLoaded() const { return m_videoLoaded; }
bool VideoPlaybackService::vehicleDetectEnabled() const { return m_vehicleDetectEnabled; }
bool VideoPlaybackService::trafficLightDetectEnabled() const { return m_trafficLightDetectEnabled; }
int VideoPlaybackService::frameCounter() const { return m_frameCounter; }
int VideoPlaybackService::currentPositionMs() const { return m_currentPositionMs; }
int VideoPlaybackService::totalDurationMs() const { return m_totalDurationMs; }

void VideoPlaybackService::setVehicleDetectEnabled(bool enabled)
{
    if (m_vehicleDetectEnabled != enabled) {
        m_vehicleDetectEnabled = enabled;
        emit vehicleDetectEnabledChanged();
    }
}

void VideoPlaybackService::setTrafficLightDetectEnabled(bool enabled)
{
    if (m_trafficLightDetectEnabled != enabled) {
        m_trafficLightDetectEnabled = enabled;
        emit trafficLightDetectEnabledChanged();
    }
}

bool VideoPlaybackService::openVideo(const QString &filePath)
{
    closeVideo();

    m_capture.open(filePath.toStdString());
    if (!m_capture.isOpened()) {
        qWarning() << "无法打开视频文件:" << filePath;
        m_videoLoaded = false;
        emit videoInfoChanged();
        return false;
    }

    m_totalFrames = static_cast<int>(m_capture.get(cv::CAP_PROP_FRAME_COUNT));
    m_fps = m_capture.get(cv::CAP_PROP_FPS);
    if (m_fps <= 0) m_fps = 30.0;
    m_frameWidth = static_cast<int>(m_capture.get(cv::CAP_PROP_FRAME_WIDTH));
    m_frameHeight = static_cast<int>(m_capture.get(cv::CAP_PROP_FRAME_HEIGHT));

    // 获取真实的视频时长（毫秒）
    // 方法：跳到文件末尾读取 CAP_PROP_POS_MSEC，再跳回开头
    // 这比 totalFrames/fps 可靠，因为 MJPEG AVI 的 totalFrames 经常虚高
    m_totalDurationMs = 0;
    m_capture.set(cv::CAP_PROP_POS_AVI_RATIO, 0.999);  // 跳到接近末尾
    double endPosMs = m_capture.get(cv::CAP_PROP_POS_MSEC);
    m_capture.set(cv::CAP_PROP_POS_AVI_RATIO, 0.0);    // 跳回开头

    if (endPosMs > 0) {
        m_totalDurationMs = static_cast<int>(endPosMs + 0.5);  // 四舍五入
        qDebug() << "真实视频时长:" << m_totalDurationMs << "ms"
                 << "(" << (m_totalDurationMs / 1000.0) << "秒)";
    } else {
        // 回退方案：用 totalFrames/fps 估算
        m_totalDurationMs = static_cast<int>(m_totalFrames * 1000.0 / m_fps);
        qDebug() << "无法获取真实时长，使用帧数估算:" << m_totalDurationMs << "ms";
    }

    // 重新读取第一帧确保位置正确
    m_capture.set(cv::CAP_PROP_POS_FRAMES, 0);

    m_videoLoaded = true;
    m_currentFrame = 0;
    m_currentPositionMs = 0;

    m_playTimer->setInterval(static_cast<int>(1000.0 / (m_fps * m_playbackSpeed)));

    emit videoInfoChanged();
    emit frameChanged();
    emit positionMsChanged();

    qDebug() << "视频已打开:" << filePath
             << m_frameWidth << "x" << m_frameHeight
             << m_fps << "fps" << m_totalFrames << "frames"
             << "时长" << (m_totalDurationMs / 1000.0) << "秒";

    return true;
}

void VideoPlaybackService::closeVideo()
{
    pause();

    if (m_capture.isOpened()) {
        m_capture.release();
    }

    m_videoLoaded = false;
    m_currentFrame = 0;
    m_totalFrames = 0;
    m_currentPositionMs = 0;
    m_totalDurationMs = 0;
    m_lastFrame.release();
    emit videoInfoChanged();
}

void VideoPlaybackService::play()
{
    if (!m_videoLoaded || !m_capture.isOpened()) return;

    m_isPlaying = true;
    m_playTimer->start();
    emit playingChanged();
}

void VideoPlaybackService::pause()
{
    m_isPlaying = false;
    m_playTimer->stop();
    emit playingChanged();
}

void VideoPlaybackService::seekToFrame(int frame)
{
    if (!m_videoLoaded || !m_capture.isOpened()) return;

    if (frame < 0) frame = 0;
    if (m_totalFrames > 0 && frame >= m_totalFrames) frame = m_totalFrames - 1;

    m_capture.set(cv::CAP_PROP_POS_FRAMES, static_cast<double>(frame));
    m_currentFrame = frame;
    m_currentPositionMs = static_cast<int>(m_capture.get(cv::CAP_PROP_POS_MSEC));
    emit frameChanged();
    emit positionMsChanged();

    if (!m_isPlaying) {
        processAndSendFrame();
    }
}

void VideoPlaybackService::seekToSec(double sec)
{
    if (!m_videoLoaded || m_fps <= 0) return;
    seekToFrame(static_cast<int>(sec * m_fps));
}

void VideoPlaybackService::setPlaybackSpeed(double speed)
{
    if (speed < 0.25) speed = 0.25;
    if (speed > 4.0) speed = 4.0;
    m_playbackSpeed = speed;
    if (m_fps > 0) {
        m_playTimer->setInterval(static_cast<int>(1000.0 / (m_fps * m_playbackSpeed)));
    }
}

QImage VideoPlaybackService::getCurrentFrameImage()
{
    if (m_lastFrame.empty()) return QImage();

    cv::Mat rgbFrame;
    cv::cvtColor(m_lastFrame, rgbFrame, cv::COLOR_BGR2RGB);
    QImage qimg(rgbFrame.data, rgbFrame.cols, rgbFrame.rows, rgbFrame.step,
                QImage::Format_RGB888);
    return qimg.copy();
}

QString VideoPlaybackService::saveScreenshot(const QString &videoDir)
{
    if (m_lastFrame.empty()) return "";

    QString screenshotDir = videoDir + "/screenshots/";
    QDir dir(screenshotDir);
    if (!dir.exists()) dir.mkpath(".");

    QDateTime now = QDateTime::currentDateTime();
    QString fileName = "播放截图_" + now.toString("yyyyMMdd_HHmmss") + ".png";
    QString filePath = screenshotDir + fileName;

    if (cv::imwrite(filePath.toStdString(), m_lastFrame)) {
        qDebug() << "播放截图保存成功:" << filePath;
        emit screenshotSaved(filePath);
        return filePath;
    } else {
        qWarning() << "播放截图保存失败:" << filePath;
        return "";
    }
}

void VideoPlaybackService::onPlayTimer()
{
    if (!m_videoLoaded || !m_capture.isOpened()) {
        pause();
        return;
    }

    processAndSendFrame();
}

void VideoPlaybackService::processAndSendFrame()
{
    cv::Mat frame;
    if (!m_capture.read(frame) || frame.empty()) {
        pause();
        m_currentFrame = m_totalFrames;
        m_currentPositionMs = m_totalDurationMs;
        emit frameChanged();
        emit positionMsChanged();
        emit playbackFinished();
        return;
    }

    m_currentFrame = static_cast<int>(m_capture.get(cv::CAP_PROP_POS_FRAMES));
    // 使用 CAP_PROP_POS_MSEC 获取真实的毫秒位置（比帧数/fps可靠）
    m_currentPositionMs = static_cast<int>(m_capture.get(cv::CAP_PROP_POS_MSEC));
    emit frameChanged();
    emit positionMsChanged();

    detectAndDrawFrame(frame);

    m_lastFrame = frame.clone();

    cv::Mat rgbFrame;
    cv::cvtColor(frame, rgbFrame, cv::COLOR_BGR2RGB);
    QImage qimg(rgbFrame.data, rgbFrame.cols, rgbFrame.rows, rgbFrame.step,
                QImage::Format_RGB888);
    QImage frameCopy = qimg.copy();

    if (m_frameProvider) {
        m_frameProvider->updateFrame("playback", frameCopy);
    }

    m_frameCounter++;
    emit frameCounterChanged();
}

void VideoPlaybackService::detectAndDrawFrame(cv::Mat &frame)
{
    // YOLOv8每2帧执行一次检测（单次推理同时获取车辆+红绿灯）
    bool needDetect = (m_vehicleDetectEnabled || m_trafficLightDetectEnabled)
                      && m_detectionEngine->isLoaded()
                      && (m_frameCounter % 2 == 0);

    if (needDetect) {
        QVariantList vehicleResults;
        TrafficLightResult tlResult;

        if (m_vehicleDetectEnabled && m_trafficLightDetectEnabled) {
            m_detectionEngine->detectAll(frame, vehicleResults, tlResult);
        } else if (m_vehicleDetectEnabled) {
            m_detectionEngine->detectVehicles(frame, vehicleResults);
        } else if (m_trafficLightDetectEnabled) {
            m_detectionEngine->detectTrafficLights(frame, tlResult);
        }

        m_lastVehicleResults = vehicleResults;
        m_lastTrafficLightRects = tlResult.trafficLightRects;
        m_lastTrafficLightState = tlResult.state;
    }

    // 绘制检测框
    if ((m_vehicleDetectEnabled || m_trafficLightDetectEnabled) && m_detectionEngine->isLoaded()) {
        m_detectionEngine->drawDetections(frame, m_lastVehicleResults,
                                           m_lastTrafficLightRects,
                                           m_lastTrafficLightState,
                                           "360 Scene Replay");
    }
}
