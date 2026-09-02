#include "ReverseRecordService.h"
#include "services/video/common/VideoFrameProvider.h"
#include "services/config/AppConfig.h"
#include <QDir>
#include <QDebug>
#include <QFileInfo>
#include <cmath>

ReverseRecordService::ReverseRecordService(VideoFrameProvider *provider, QObject *parent)
    : QObject(parent)
    , m_camOpened(false)
    , m_autoRecording(false)
    , m_autoRecordEnabled(false)   // 默认关闭自动录制
    , m_autoRecordIntervalSec(10)
    , m_targetFrames(300)
    , m_currentFrameCount(0)
    , m_videoDir(AppConfig::instance().videoDir())
    , m_running(false)
    , m_captureTimer(new QTimer(this))
    , m_autoRecordTimer(new QTimer(this))
    , m_steerAnimTimer(new QTimer(this))
    , m_warningLevel("safe")
    , m_obstacleDist(-1.0)
    , m_hasCarRear(false)
    , m_carRearTopRatio(1.0)
    , m_steeringAngle(0.0)
    , m_targetSteerAngle(0.0)
    , m_useStaticImage(false)
    , m_useSecondImage(false)
    , m_warningImgLoaded(false)
    , m_frameProvider(provider)
    , m_frameCounter(0)
{
    m_captureTimer->setInterval(33); // ~30fps
    connect(m_captureTimer, &QTimer::timeout, this, &ReverseRecordService::onCaptureTimer);

    m_autoRecordTimer->setInterval(m_autoRecordIntervalSec * 1000);
    connect(m_autoRecordTimer, &QTimer::timeout, this, &ReverseRecordService::onAutoRecordTimer);

    // 转向动画定时器：平滑过渡到目标角度
    m_steerAnimTimer->setInterval(30); // ~33fps动画
    connect(m_steerAnimTimer, &QTimer::timeout, this, [this]() {
        qreal diff = m_targetSteerAngle - m_steeringAngle;
        if (std::abs(diff) < 0.02) {
            m_steeringAngle = m_targetSteerAngle;
            m_steerAnimTimer->stop();
        } else {
            m_steeringAngle += diff * 0.15; // 缓动系数
        }
        emit steeringAngleChanged();
    });

    // 加载警告图片（支持alpha通道，从 resources/images/ 查找）
    {
        QString warnPath = AppConfig::instance().resolveResource("images/warning.png");
        cv::Mat warnImg = warnPath.isEmpty() ? cv::Mat() : cv::imread(warnPath.toStdString(), cv::IMREAD_UNCHANGED);
        if (!warnImg.empty()) {
            m_warningImage = warnImg.clone();
            m_warningImgLoaded = true;
            qDebug() << "警告图片加载成功:" << warnImg.cols << "x" << warnImg.rows
                     << "通道数:" << warnImg.channels();
        } else {
            qWarning() << "无法加载警告图片，请将 warning.png 放到 resources/images/ 目录";
        }
    }

    // 确保视频目录存在
    QDir dir(m_videoDir);
    if (!dir.exists()) dir.mkpath(".");

    // 自动加载两张倒车测试图片（有障碍/无障碍，从 resources/test_images/ 查找）
    {
        // 主图：carafter.png（后方摄像头，可能有障碍物）
        QString img1Path = AppConfig::instance().resolveResource("test_images/carafter.png");
        cv::Mat img1 = img1Path.isEmpty() ? cv::Mat() : cv::imread(img1Path.toStdString());
        if (!img1.empty()) {
            m_staticImage = img1.clone();
            m_useStaticImage = true;
            qDebug() << "倒车：加载主图(有障碍)" << img1.cols << "x" << img1.rows;
        }

        // 第二张图：Rear_viewCar.png（后方摄像头，无障碍物）
        QString img2Path = AppConfig::instance().resolveResource("test_images/Rear_viewCar.png");
        cv::Mat img2 = img2Path.isEmpty() ? cv::Mat() : cv::imread(img2Path.toStdString());
        if (!img2.empty()) {
            m_staticImage2 = img2.clone();
            qDebug() << "倒车：加载第二张图(无障碍)" << img2.cols << "x" << img2.rows;
        }
    }
}

ReverseRecordService::~ReverseRecordService()
{
    stop();
    closeCamera();
}

bool ReverseRecordService::isAutoRecording() const { return m_autoRecording; }
bool ReverseRecordService::isAutoRecordEnabled() const { return m_autoRecordEnabled; }
int ReverseRecordService::autoRecordIntervalSec() const { return m_autoRecordIntervalSec; }
int ReverseRecordService::targetFrames() const { return m_targetFrames; }
int ReverseRecordService::currentFrameCount() const { return m_currentFrameCount; }
QString ReverseRecordService::videoDir() const { return m_videoDir; }
bool ReverseRecordService::isRunning() const { return m_running; }
QString ReverseRecordService::warningLevel() const { return m_warningLevel; }
qreal ReverseRecordService::steeringAngle() const { return m_steeringAngle; }
bool ReverseRecordService::useStaticImage() const { return m_useStaticImage; }
QString ReverseRecordService::staticImagePath() const { return m_staticImagePath; }
int ReverseRecordService::frameCounter() const { return m_frameCounter; }

bool ReverseRecordService::useSecondImage() const { return m_useSecondImage; }
void ReverseRecordService::setUseSecondImage(bool use) {
    if (m_useSecondImage != use) {
        m_useSecondImage = use;
        emit secondImageChanged();
        qDebug() << "倒车：切换到" << (use ? "第二张图(无障碍)" : "主图(有障碍)");
    }
}

void ReverseRecordService::setSteeringAngle(qreal angle)
{
    if (angle < -1.0) angle = -1.0;
    if (angle > 1.0) angle = 1.0;
    if (!qFuzzyCompare(m_steeringAngle, angle)) {
        m_steeringAngle = angle;
        emit steeringAngleChanged();
    }
}

void ReverseRecordService::setUseStaticImage(bool use)
{
    if (m_useStaticImage != use) {
        m_useStaticImage = use;
        emit useStaticImageChanged();
    }
}

void ReverseRecordService::setAutoRecordEnabled(bool enabled)
{
    if (m_autoRecordEnabled != enabled) {
        m_autoRecordEnabled = enabled;
        emit autoRecordEnabledChanged();
    }
}

void ReverseRecordService::setAutoRecordIntervalSec(int sec)
{
    if (sec < 5) sec = 5;
    if (sec > 60) sec = 60;
    if (m_autoRecordIntervalSec != sec) {
        m_autoRecordIntervalSec = sec;
        m_autoRecordTimer->setInterval(sec * 1000);
        emit autoRecordIntervalChanged();
    }
}

void ReverseRecordService::setTargetFrames(int frames)
{
    if (frames < 100) frames = 100;
    if (frames > 1000) frames = 1000;
    if (m_targetFrames != frames) {
        m_targetFrames = frames;
        emit targetFramesChanged();
    }
}

void ReverseRecordService::setVideoDir(const QString &dir)
{
    if (m_videoDir != dir) {
        m_videoDir = dir;
        QDir d(dir);
        if (!d.exists()) d.mkpath(".");
        emit videoDirChanged();
    }
}

// ============================================================================
// 转向控制
// ============================================================================
void ReverseRecordService::steerLeft()
{
    m_targetSteerAngle = -1.0;
    if (!m_steerAnimTimer->isActive()) {
        m_steerAnimTimer->start();
    }
}

void ReverseRecordService::steerRight()
{
    m_targetSteerAngle = 1.0;
    if (!m_steerAnimTimer->isActive()) {
        m_steerAnimTimer->start();
    }
}

void ReverseRecordService::steerCenter()
{
    m_targetSteerAngle = 0.0;
    if (!m_steerAnimTimer->isActive()) {
        m_steerAnimTimer->start();
    }
}

bool ReverseRecordService::loadStaticImage(const QString &path)
{
    cv::Mat img = cv::imread(path.toStdString());
    if (img.empty()) {
        qWarning() << "无法加载车尾照片:" << path;
        return false;
    }

    m_staticImage = img.clone();
    m_staticImagePath = path;
    m_useStaticImage = true;
    emit useStaticImageChanged();
    emit staticImageChanged();

    qDebug() << "车尾照片加载成功:" << path
             << "尺寸:" << img.cols << "x" << img.rows;
    return true;
}

// ============================================================================
// 服务启停
// ============================================================================
bool ReverseRecordService::start()
{
    if (m_running) return true;

    m_running = true;
    m_captureTimer->start();

    if (m_autoRecordEnabled) {
        m_autoRecordTimer->start();
    }

    emit runningChanged();
    qDebug() << "倒车模式服务启动";
    return true;
}

void ReverseRecordService::stop()
{
    if (!m_running) return;

    m_running = false;
    m_captureTimer->stop();
    m_autoRecordTimer->stop();
    m_steerAnimTimer->stop();

    if (m_autoRecording) {
        stopAutoRecord();
    }

    emit runningChanged();
    qDebug() << "倒车模式服务停止";
}

bool ReverseRecordService::openCamera(int camId)
{
    m_capture.open(camId);
    if (!m_capture.isOpened()) {
        qWarning() << "无法打开倒车摄像头，设备ID:" << camId;
        return false;
    }

    m_camOpened = true;
    qDebug() << "倒车摄像头已打开，设备ID:" << camId;
    return true;
}

void ReverseRecordService::closeCamera()
{
    if (m_capture.isOpened()) {
        m_capture.release();
    }
    m_camOpened = false;
    qDebug() << "倒车摄像头已关闭";
}

// ============================================================================
// 帧处理
// ============================================================================
void ReverseRecordService::onCaptureTimer()
{
    if (m_running) {
        processFrame();
    }
}

void ReverseRecordService::processFrame()
{
    cv::Mat frame;

    if (m_useStaticImage) {
        // 根据切换标志选择主图(有障碍)或第二张图(无障碍)
        if (m_useSecondImage && !m_staticImage2.empty()) {
            frame = m_staticImage2.clone();
        } else if (!m_staticImage.empty()) {
            frame = m_staticImage.clone();
        } else {
            return;
        }
    } else if (m_camOpened && m_capture.isOpened()) {
        if (!m_capture.read(frame) || frame.empty()) {
            return;
        }
    } else {
        return;
    }

    // 先检测画面中是否有车屁股（确定边界位置，供障碍物检测和辅助线使用）
    m_hasCarRear = detectCarRear(frame);

    // 障碍物检测：仅在使用主图(有障碍)或摄像头时运行
    // 使用第二张图(无障碍)时跳过检测，强制warningLevel="safe"
    if (m_useSecondImage) {
        if (m_warningLevel != "safe") {
            m_warningLevel = "safe";
            emit warningLevelChanged();
        }
        m_obstacleDist = -1.0;
    } else {
        detectObstacles(frame);
    }

    // 绘制辅助线（根据是否有障碍物选择完全不同的两套逻辑）
    drawAuxiliaryLines(frame);

    // ========== 以下警告叠加逻辑仅在有障碍物时生效 ==========
    // 无障碍物(warningLevel=="safe")时不显示任何警告
    bool hasObstacle = (m_warningLevel == "danger" || m_warningLevel == "warning");

    // 障碍物进入危险区时，画面顶部叠加红色WARNING
    if (hasObstacle && m_warningLevel == "danger") {
        int W = frame.cols, H = frame.rows;
        // 半透明红色背景条
        cv::Mat overlay(H * 0.08, W, CV_8UC3, cv::Scalar(0, 0, 200));
        cv::Mat roi = frame(cv::Rect(0, 0, W, static_cast<int>(H * 0.08)));
        cv::addWeighted(overlay, 0.6, roi, 0.4, 0, roi);
        // 白色WARNING文字
        cv::putText(frame, "WARNING", cv::Point(W / 2 - 160, static_cast<int>(H * 0.06)),
                    cv::FONT_HERSHEY_DUPLEX, 2.0, cv::Scalar(255, 255, 255), 3, cv::LINE_AA);
    }

    // ========== 警告图片叠加（左上角）==========
    // 仅在有障碍物时显示，无障碍物时不显示任何警告
    if (hasObstacle && m_warningImgLoaded && !m_warningImage.empty()
        && (m_warningLevel == "danger" || m_warningLevel == "warning")) {
        // 缩放警告图片到合适大小（宽度为画面宽度的1/3）
        int warnW = frame.cols / 3;
        double scale = static_cast<double>(warnW) / m_warningImage.cols;
        int warnH = static_cast<int>(m_warningImage.rows * scale);
        cv::Mat warnResized;
        cv::resize(m_warningImage, warnResized, cv::Size(warnW, warnH));

        // 确保ROI不越界
        int roiX = 10;  // 左上角，10像素边距
        int roiY = 10;
        if (roiX + warnW > frame.cols) warnW = frame.cols - roiX;
        if (roiY + warnH > frame.rows) warnH = frame.rows - roiY;

        if (m_warningImage.channels() == 4) {
            // BGRA格式：带alpha通道，需要透明度混合
            // 先将缩放后的警告图从BGRA分离
            cv::Mat warnRGB;
            cv::Mat warnAlpha;
            cv::Mat warnResizedROI = warnResized(cv::Rect(0, 0, warnW, warnH));
            cv::Mat channels[4];
            cv::split(warnResizedROI, channels);
            warnRGB = cv::Mat(warnResizedROI.rows, warnResizedROI.cols, CV_8UC3);
            cv::merge(std::vector<cv::Mat>{channels[0], channels[1], channels[2]}, warnRGB);
            warnAlpha = channels[3];

            // 对frame的ROI区域做alpha混合
            cv::Mat frameROI = frame(cv::Rect(roiX, roiY, warnW, warnH));
            for (int y = 0; y < warnH; y++) {
                for (int x = 0; x < warnW; x++) {
                    uchar alpha = warnAlpha.at<uchar>(y, x);
                    if (alpha == 0) continue;  // 完全透明，跳过
                    double a = alpha / 255.0;
                    cv::Vec3b &dst = frameROI.at<cv::Vec3b>(y, x);
                    cv::Vec3b src = warnRGB.at<cv::Vec3b>(y, x);
                    dst[0] = static_cast<uchar>(src[0] * a + dst[0] * (1.0 - a));
                    dst[1] = static_cast<uchar>(src[1] * a + dst[1] * (1.0 - a));
                    dst[2] = static_cast<uchar>(src[2] * a + dst[2] * (1.0 - a));
                }
            }
        } else {
            // 无alpha通道：直接覆盖绘制（加半透明边框效果）
            cv::Mat overlay = frame.clone();
            cv::Mat warnResizedROI = warnResized(cv::Rect(0, 0, warnW, warnH));
            warnResizedROI.copyTo(overlay(cv::Rect(roiX, roiY, warnW, warnH)));
            cv::addWeighted(overlay, 0.85, frame, 0.15, 0, frame);
            // 绘制红色边框
            cv::rectangle(frame, cv::Rect(roiX - 2, roiY - 2, warnW + 4, warnH + 4),
                          cv::Scalar(0, 0, 255), 2);
        }

        // 危险级别时额外闪烁WARNING文字（仅在有障碍物时）
        if (hasObstacle && m_warningLevel == "danger") {
            // 使用帧计数实现闪烁效果（每15帧切换）
            if ((m_frameCounter / 15) % 2 == 0) {
                int textX = roiX;
                int textY = roiY + warnH + 30;
                cv::putText(frame, "WARNING!", cv::Point(textX, textY),
                            cv::FONT_HERSHEY_SIMPLEX, 1.0, cv::Scalar(0, 0, 255), 3);
            }
        }
    }

    // 绘制水印（左下角）
    cv::putText(frame, "360 Reverse Mode",
                cv::Point(10, frame.rows - 10),
                cv::FONT_HERSHEY_SIMPLEX, 0.6, cv::Scalar(255, 255, 255), 1);
    QString timeStr = QDateTime::currentDateTime().toString("yyyy-MM-dd HH:mm:ss");
    cv::putText(frame, timeStr.toStdString(),
                cv::Point(10, frame.rows - 30),
                cv::FONT_HERSHEY_SIMPLEX, 0.4, cv::Scalar(255, 255, 255), 1);

    // 自动录制
    if (m_autoRecording && m_writer.isOpened()) {
        m_writer.write(frame);
        m_currentFrameCount++;
        emit currentFrameCountChanged();

        if (m_currentFrameCount >= m_targetFrames) {
            stopAutoRecord();
        }
    }

    // 发送帧到QML（通过VideoFrameProvider）
    cv::Mat rgbFrame;
    cv::cvtColor(frame, rgbFrame, cv::COLOR_BGR2RGB);
    QImage qimg(rgbFrame.data, rgbFrame.cols, rgbFrame.rows, rgbFrame.step,
                QImage::Format_RGB888);
    QImage frameCopy = qimg.copy();

    if (m_frameProvider) {
        m_frameProvider->updateFrame("reverse", frameCopy);
    }

    m_frameCounter++;
    emit frameCounterChanged();
}

// ============================================================================
// 自动录制
// ============================================================================
void ReverseRecordService::onAutoRecordTimer()
{
    if (m_autoRecordEnabled && m_running && !m_autoRecording) {
        startAutoRecord();
    }
}

void ReverseRecordService::startAutoRecord()
{
    QDateTime now = QDateTime::currentDateTime();
    QString fileName = "倒车记录_" + now.toString("yyyyMMdd_HHmmss") + ".avi";
    QString filePath = m_videoDir + "/" + fileName;

    double fps = 30.0;
    int frameWidth = 640;
    int frameHeight = 480;

    if (m_useStaticImage && !m_staticImage.empty()) {
        frameWidth = m_staticImage.cols;
        frameHeight = m_staticImage.rows;
    } else if (m_camOpened && m_capture.isOpened()) {
        fps = m_capture.get(cv::CAP_PROP_FPS);
        if (fps <= 0) fps = 30.0;
        frameWidth = static_cast<int>(m_capture.get(cv::CAP_PROP_FRAME_WIDTH));
        frameHeight = static_cast<int>(m_capture.get(cv::CAP_PROP_FRAME_HEIGHT));
    }

    int fourcc = cv::VideoWriter::fourcc('M', 'J', 'P', 'G');
    m_writer.open(filePath.toStdString(), fourcc, fps,
                  cv::Size(frameWidth, frameHeight), true);

    if (!m_writer.isOpened()) {
        qWarning() << "无法创建倒车视频文件:" << filePath;
        return;
    }

    m_autoRecording = true;
    m_currentFrameCount = 0;
    emit autoRecordingChanged();
    emit currentFrameCountChanged();

    qDebug() << "开始倒车自动录制:" << filePath
             << "目标帧数:" << m_targetFrames;
}

void ReverseRecordService::stopAutoRecord()
{
    if (!m_autoRecording) return;

    m_autoRecording = false;
    int frames = m_currentFrameCount;

    if (m_writer.isOpened()) {
        m_writer.release();
    }

    m_currentFrameCount = 0;
    emit autoRecordingChanged();
    emit currentFrameCountChanged();

    QDateTime now = QDateTime::currentDateTime();
    QString fileName = "倒车记录_" + now.toString("yyyyMMdd_HHmmss") + ".avi";
    QString filePath = m_videoDir + "/" + fileName;

    emit recordingSaved(filePath, frames);
    qDebug() << "倒车自动录制完成，帧数:" << frames;
}



// ============================================================================
// 动态转向辅助线 - 两种完全独立的模式
//
// ★ 无障碍模式 (warningLevel == "safe")：
//   与a.c参考版本完全一致，底部在~70%画面高度，轨迹长，透视收缩
//   不显示任何警告
//
// ★ 有障碍模式 (warningLevel != "safe")：
//   底部在画面最底部，宽度3/4画面，前端在车底
//   远端根据障碍物距离动态缩短，障碍物越近轨迹越短
//   颜色随距离变化：0~1m红，1~3m黄，3m+绿
//   显示WARNING告警
//
// 世界坐标系：
//   原点 = 后轴中心在地面的投影
//   X轴 = 车身横向（正方向朝右）
//   Y轴 = 车身前方（正方向远离车尾）
//
// steeringAngle: -1.0(左满) ~ 0(直) ~ 1.0(右满)
// ============================================================================
void ReverseRecordService::drawAuxiliaryLines(cv::Mat &frame)
{
    if (frame.empty()) return;

    int W = frame.cols;
    int H = frame.rows;
    int centerX = W / 2;
    qreal angle = m_steeringAngle;

    // 转向角映射：-1~1 → 实际转向角（最大35度）
    const double maxSteerRad = 35.0 * M_PI / 180.0;
    const double wheelBase = 2.7;
    double steerRad = angle * maxSteerRad;
    double R = (std::abs(steerRad) < 0.001) ? 1e8 : wheelBase / std::tan(steerRad);

    bool hasObstacle = (m_warningLevel == "danger" || m_warningLevel == "warning");

    // ================================================================
    //  根据是否有障碍物，选择完全不同的参数和IPM模型
    // ================================================================
    double trackWidth, halfTrack, bandWidth, halfBand;
    double minDist, maxDist;
    double dangerEnd, warningEnd;
    int numPts;

    // 公共IPM参数
    const double horizonRatio = 0.15;   // 地平线在画面15%处

    // IPM投影函数（两种模式不同）
    std::function<double(double)> ipmV;
    std::function<double(double, double)> ipmU;

    if (!hasObstacle) {
        // ========== 无障碍模式：完全照搬a.c ==========
        trackWidth = 1.6;   halfTrack = 0.8;
        bandWidth  = 0.11;  halfBand  = 0.055;
        minDist    = 0.3;
        maxDist    = 4.0;
        numPts     = 60;

        // 固定距离分区
        dangerEnd  = 1.0;
        warningEnd = 3.0;

        // a.c的IPM有理函数模型（底部位置和轨迹长度经过验证）
        // ipmV = H*(0.15 + 0.85/(1+Y*1.8))
        // Y=0.3m → 70.2%画面高度（轨迹底部）
        // Y=4.0m → 25.4%画面高度（轨迹远端）
        ipmV = [&](double Y_m) -> double {
            return H * (horizonRatio + (1.0 - horizonRatio) / (1.0 + Y_m * 1.8));
        };
        ipmU = [&](double X_m, double Y_m) -> double {
            return centerX + X_m * W * 0.42 / (Y_m * 0.85 + 0.45);
        };
    } else {
        // ========== 有障碍模式：底部在画面底部，宽度3/4 ==========
        trackWidth = 1.4;   halfTrack = 0.7;
        bandWidth  = 0.11;  halfBand  = 0.055;
        minDist    = 0.01;  // 从画面最底部开始（紧贴车底）
        maxDist    = 4.0;   // 默认4米
        numPts     = 60;

        // 动态缩短：障碍物越近，轨迹远端越短
        if (m_obstacleDist > 0 && m_obstacleDist < maxDist) {
            maxDist = m_obstacleDist;
            if (maxDist < minDist + 0.3)
                maxDist = minDist + 0.3;
        }

        // 固定距离分区（颜色随障碍物距离动态变化）
        dangerEnd  = 1.0;
        warningEnd = 3.0;
        if (dangerEnd > maxDist) dangerEnd = maxDist;
        if (warningEnd > maxDist) warningEnd = maxDist;

        // 有障碍模式IPM：bottomRatio=1.0（画面底部），缩窄横向使宽度约3/4画面
        // ipmV = H*(0.15+0.85/(1+Y*1.8))  → Y=0→画面底部，Y→∞→15%
        // ipmU scaleFactor=0.24 → 底部宽度约3/4画面
        ipmV = [&](double Y_m) -> double {
            return H * (horizonRatio + (1.0 - horizonRatio) / (1.0 + Y_m * 1.8));
        };
        ipmU = [&](double X_m, double Y_m) -> double {
            return centerX + X_m * W * 0.24 / (Y_m * 0.85 + 0.45);
        };
    }

    // ================================================================
    //  以下为公共绘制逻辑（两种模式共用）
    // ================================================================

    // 生成4条边界轨迹
    std::vector<cv::Point> leftOuter, leftInner, rightInner, rightOuter;
    for (int i = 0; i < numPts; i++) {
        double t = static_cast<double>(i) / (numPts - 1);
        double Y = minDist + t * (maxDist - minDist);

        double X_offset = 0;
        if (std::abs(R) < 1e6) {
            double ratio = Y / R;
            if (std::abs(ratio) < 1.0)
                X_offset = R * (1.0 - std::cos(ratio));
            else
                X_offset = R;
        }

        double X_LO = -halfTrack - halfBand + X_offset;
        double X_LI = -halfTrack + halfBand + X_offset;
        double X_RI =  halfTrack - halfBand + X_offset;
        double X_RO =  halfTrack + halfBand + X_offset;

        int v = static_cast<int>(ipmV(Y));
        leftOuter.push_back(cv::Point(static_cast<int>(ipmU(X_LO, Y)), v));
        leftInner.push_back(cv::Point(static_cast<int>(ipmU(X_LI, Y)), v));
        rightInner.push_back(cv::Point(static_cast<int>(ipmU(X_RI, Y)), v));
        rightOuter.push_back(cv::Point(static_cast<int>(ipmU(X_RO, Y)), v));
    }

    // 分区边界索引
    auto findIndex = [&](double distY) -> int {
        for (int i = 0; i < numPts; i++) {
            double t = static_cast<double>(i) / (numPts - 1);
            double Y = minDist + t * (maxDist - minDist);
            if (Y >= distY) return i;
        }
        return numPts - 1;
    };
    int idxDangerEnd = findIndex(dangerEnd);
    int idxWarningEnd = findIndex(warningEnd);

    // 色块填充
    auto fillBand = [&](const std::vector<cv::Point> &inner,
                        const std::vector<cv::Point> &outer,
                        int startIdx, int endIdx,
                        const cv::Scalar &color, double alpha) {
        if (endIdx <= startIdx) return;
        cv::Mat overlay = frame.clone();
        std::vector<cv::Point> poly;
        for (int i = startIdx; i <= endIdx && i < static_cast<int>(inner.size()); i++)
            poly.push_back(inner[i]);
        for (int i = std::min(endIdx, static_cast<int>(outer.size()) - 1); i >= startIdx; i--)
            poly.push_back(outer[i]);
        if (poly.size() >= 3) {
            cv::fillConvexPoly(overlay, poly, color);
            cv::addWeighted(overlay, alpha, frame, 1.0 - alpha, 0, frame);
        }
    };

    cv::Scalar colorRed(0, 0, 220);
    cv::Scalar colorYellow(0, 210, 255);
    cv::Scalar colorGreen(0, 180, 0);
    double alpha = 0.7;

    // 左轮色块
    if (dangerEnd > minDist)
        fillBand(leftInner, leftOuter, 0, idxDangerEnd, colorRed, alpha);
    if (warningEnd > dangerEnd && idxWarningEnd > idxDangerEnd)
        fillBand(leftInner, leftOuter, idxDangerEnd, idxWarningEnd, colorYellow, alpha);
    if (maxDist > warningEnd && (numPts - 1) > idxWarningEnd)
        fillBand(leftInner, leftOuter, idxWarningEnd, numPts - 1, colorGreen, alpha);

    // 右轮色块
    if (dangerEnd > minDist)
        fillBand(rightInner, rightOuter, 0, idxDangerEnd, colorRed, alpha);
    if (warningEnd > dangerEnd && idxWarningEnd > idxDangerEnd)
        fillBand(rightInner, rightOuter, idxDangerEnd, idxWarningEnd, colorYellow, alpha);
    if (maxDist > warningEnd && (numPts - 1) > idxWarningEnd)
        fillBand(rightInner, rightOuter, idxWarningEnd, numPts - 1, colorGreen, alpha);

    // 白色描边
    auto drawPolyline = [&](const std::vector<cv::Point> &pts, int start, int end,
                            const cv::Scalar &color, int thick) {
        for (int i = start; i < end && i < static_cast<int>(pts.size()) - 1; i++)
            cv::line(frame, pts[i], pts[i + 1], color, thick, cv::LINE_AA);
    };
    drawPolyline(leftInner, 0, numPts - 1, cv::Scalar(255, 255, 255), 1);
    drawPolyline(leftOuter, 0, numPts - 1, cv::Scalar(255, 255, 255), 1);
    drawPolyline(rightInner, 0, numPts - 1, cv::Scalar(255, 255, 255), 1);
    drawPolyline(rightOuter, 0, numPts - 1, cv::Scalar(255, 255, 255), 1);

    // 阶梯拐角横向标识
    auto drawStepMarker = [&](int idx, const cv::Scalar &color) {
        if (idx <= 0 || idx >= numPts) return;
        cv::line(frame, leftInner[idx], leftOuter[idx], color, 2, cv::LINE_AA);
        cv::line(frame, rightInner[idx], rightOuter[idx], color, 2, cv::LINE_AA);

        cv::Point liPt = leftInner[idx], riPt = rightInner[idx];
        int dx = riPt.x - liPt.x, dy = riPt.y - liPt.y;
        float len = std::sqrt(static_cast<float>(dx * dx + dy * dy));
        int numDash = static_cast<int>(len / 12);
        for (int d = 0; d < numDash; d++) {
            qreal t1 = static_cast<qreal>(d * 2) / (numDash * 2);
            qreal t2 = static_cast<qreal>(d * 2 + 1) / (numDash * 2);
            if (t2 > 1.0) t2 = 1.0;
            cv::Point p1(liPt.x + static_cast<int>(dx * t1), liPt.y + static_cast<int>(dy * t1));
            cv::Point p2(liPt.x + static_cast<int>(dx * t2), liPt.y + static_cast<int>(dy * t2));
            cv::line(frame, p1, p2, color, 1, cv::LINE_AA);
        }

        int labelX = rightOuter[idx].x + 6;
        int labelY = rightOuter[idx].y + 4;
        if (labelX + 25 > W) labelX = leftOuter[idx].x - 30;
        double distM = minDist + static_cast<double>(idx) / (numPts - 1) * (maxDist - minDist);
        cv::putText(frame, std::to_string(static_cast<int>(distM)) + "m",
                    cv::Point(labelX, labelY), cv::FONT_HERSHEY_SIMPLEX, 0.5, color, 1);
    };

    if (idxDangerEnd > 0 && idxDangerEnd < numPts)
        drawStepMarker(idxDangerEnd, cv::Scalar(0, 220, 255));  // 红黄分界(1m)
    if (idxWarningEnd > 0 && idxWarningEnd < numPts && idxWarningEnd != idxDangerEnd)
        drawStepMarker(idxWarningEnd, cv::Scalar(0, 200, 0));    // 黄绿分界(3m)

    // 底部宽度标线
    cv::line(frame, leftOuter[0], rightOuter[0],
             cv::Scalar(0, 200, 255), 2, cv::LINE_AA);

    // 中心虚线引导
    for (int i = 0; i < numPts - 3; i += 5) {
        cv::Point mid1((leftInner[i].x + rightInner[i].x) / 2, leftInner[i].y);
        cv::Point mid2((leftInner[i + 3].x + rightInner[i + 3].x) / 2, leftInner[i + 3].y);
        cv::line(frame, mid1, mid2, cv::Scalar(160, 160, 160), 1, cv::LINE_AA);
    }

    // 方向文字指示
    QString dirText;
    if (angle < -0.05)
        dirText = QString("← %1%").arg(static_cast<int>(-angle * 100));
    else if (angle > 0.05)
        dirText = QString("%1% →").arg(static_cast<int>(angle * 100));
    else
        dirText = "↑";
    cv::putText(frame, dirText.toStdString(),
                cv::Point(centerX - 30, static_cast<int>(H * 0.12)),
                cv::FONT_HERSHEY_SIMPLEX, 0.8, cv::Scalar(255, 255, 255), 2);
}

// ============================================================================
void ReverseRecordService::detectObstacles(const cv::Mat &frame)
{
    if (frame.empty()) return;

    int height = frame.rows;
    int width = frame.cols;

    // IPM参数（与drawAuxiliaryLines一致）
    const double horizonRatio = 0.15;   // 与drawAuxiliaryLines一致
    const double ipmK = 1.8;            // 有理函数衰减系数

    // 根据车屁股检测结果确定bottomRatio
    double bottomRatio = 1.0;
    if (m_hasCarRear && m_carRearTopRatio < 1.0) {
        bottomRatio = m_carRearTopRatio;
    }

    const int groundTop = static_cast<int>(height * horizonRatio);

    cv::Mat gray, blurred, edges;
    cv::cvtColor(frame, gray, cv::COLOR_BGR2GRAY);
    cv::GaussianBlur(gray, blurred, cv::Size(5, 5), 0);
    cv::Canny(blurred, edges, 50, 150);

    std::vector<std::vector<cv::Point>> contours;
    cv::findContours(edges, contours, cv::RETR_EXTERNAL, cv::CHAIN_APPROX_SIMPLE);

    QString newLevel = "safe";
    double closestDist = -1.0;  // 最近的障碍物距离(米)

    // 遍历轮廓，找到最近的障碍物
    for (const auto &contour : contours) {
        double area = cv::contourArea(contour);
        if (area < 500) continue;

        cv::Rect boundRect = cv::boundingRect(contour);
        int obstacleBottom = boundRect.y + boundRect.height;

        // 只考虑地面区域内的障碍物（地平线以下）
        if (obstacleBottom < groundTop) continue;

        // 逆IPM: 将图像Y坐标转换为距离(米)
        // 正向: ipmV(Y_m) = H * (horizonRatio + (bottomRatio - horizonRatio) / (1 + Y_m * k))
        // 逆向: Y_m = ((bottomRatio - horizonRatio) / (v/H - horizonRatio) - 1) / k
        double v = static_cast<double>(obstacleBottom);
        double vRatio = v / height;   // v/H
        double denom = vRatio - horizonRatio;
        if (denom <= 0.001) denom = 0.001;  // 防止除以0（接近地平线的点距离极远）
        double dist_m = ((bottomRatio - horizonRatio) / denom - 1.0) / ipmK;
        if (dist_m < 0) dist_m = 0;

        // 记录最近的障碍物
        if (closestDist < 0 || dist_m < closestDist) {
            closestDist = dist_m;
        }

        // 判断警告级别（距离越近越危险）
        // 与辅助线分区一致：0~1m=danger, 1~3m=warning, >3m=safe
        if (dist_m <= 1.0) {
            newLevel = "danger";
        } else if (dist_m <= 3.0) {
            if (newLevel != "danger") {
                newLevel = "warning";
            }
        }
    }

    // 更新障碍物距离
    m_obstacleDist = closestDist;

    if (newLevel != m_warningLevel) {
        m_warningLevel = newLevel;
        emit warningLevelChanged();
    }
}

// ============================================================================
// 车屁股检测 — 判断画面底部是否有车辆后部（车屁股），并检测边界位置
// 原理：车屁股通常是画面底部的大面积深色区域（保险杠/车尾），
//       与路面（较亮）形成明显亮度分界
// 返回值：是否检测到车屁股
// 副作用：更新 m_carRearTopRatio（车屁股边界在画面中的高度比例）
//         有车屁股时 m_carRearTopRatio < 1.0（如0.82表示边界在82%高度处）
//         无车屁股时 m_carRearTopRatio = 1.0（辅助线从画面底部开始）
// ============================================================================
bool ReverseRecordService::detectCarRear(const cv::Mat &frame)
{
    if (frame.empty()) {
        m_carRearTopRatio = m_carRearTopRatio * 0.3 + 1.0 * 0.7;
        return false;
    }

    int H = frame.rows;
    int W = frame.cols;

    // 转灰度图
    cv::Mat gray;
    cv::cvtColor(frame, gray, cv::COLOR_BGR2GRAY);

    // 计算每行平均亮度（用于定位车屁股边界）
    std::vector<double> rowAvg(H);
    for (int y = 0; y < H; y++) {
        rowAvg[y] = cv::mean(gray.row(y))[0];
    }

    // 底部20%区域的平均亮度
    int bottomStart = static_cast<int>(H * 0.80);
    double bottomSum = 0;
    int bottomCount = 0;
    for (int y = bottomStart; y < H; y++) {
        bottomSum += rowAvg[y];
        bottomCount++;
    }
    double bottomMean = bottomSum / bottomCount;

    // 路面参考区域（50%~70%高度）平均亮度
    int roadStart = static_cast<int>(H * 0.50);
    int roadEnd = static_cast<int>(H * 0.70);
    double roadSum = 0;
    int roadCount = 0;
    for (int y = roadStart; y < roadEnd; y++) {
        roadSum += rowAvg[y];
        roadCount++;
    }
    double roadMean = roadSum / roadCount;

    // 车屁股判断条件：
    // 1. 底部区域明显比路面暗（亮度差>40）
    // 2. 底部区域本身较暗（亮度<80）
    bool hasCarRear = (roadMean - bottomMean > 40) && (bottomMean < 80);

    if (!hasCarRear) {
        // 无车屁股 - 平滑过渡到1.0（画面底部）
        m_carRearTopRatio = m_carRearTopRatio * 0.3 + 1.0 * 0.7;
        return false;
    }

    // 有车屁股 - 从底部向上扫描，找到车屁股边界（暗→亮的过渡行）
    // 使用亮度阈值 = (底部亮度 + 路面亮度) / 2 作为分界
    double brightThreshold = (bottomMean + roadMean) / 2.0;
    int boundaryRow = static_cast<int>(H * 0.85);  // 默认值（底线保护）

    // 用5行滑动窗口平滑亮度，避免噪声导致误判
    const int smoothWindow = 5;
    for (int y = H - 1; y >= static_cast<int>(H * 0.3); y--) {
        int yStart = std::max(0, y - smoothWindow / 2);
        int yEnd = std::min(H - 1, y + smoothWindow / 2);
        double smoothBright = 0;
        int count = 0;
        for (int sy = yStart; sy <= yEnd; sy++) {
            smoothBright += rowAvg[sy];
            count++;
        }
        smoothBright /= count;

        if (smoothBright > brightThreshold) {
            boundaryRow = y;
            break;
        }
    }

    double newRatio = static_cast<double>(boundaryRow) / H;

    // 指数平滑，防止边界在帧间闪烁（0.3/0.7 快速收敛，约3帧稳定）
    m_carRearTopRatio = m_carRearTopRatio * 0.3 + newRatio * 0.7;

    // 限制合理范围：车屁股边界应在画面20%~95%之间
    // 下限20%确保不低于地平线(15%)，上限95%确保有足够画面空间绘制轨迹
    if (m_carRearTopRatio < 0.20) m_carRearTopRatio = 0.20;
    if (m_carRearTopRatio > 0.95) m_carRearTopRatio = 0.95;

    qDebug() << "检测到车屁股: 边界比例=" << m_carRearTopRatio
             << "边界行=" << boundaryRow
             << "底部亮度=" << bottomMean << "路面亮度=" << roadMean;

    return true;
}
