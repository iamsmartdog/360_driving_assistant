#include "PlaybackDetectService.h"
#include "services/video/common/DetectionEngine.h"
#include <QDir>
#include <QDebug>
#include <QDateTime>

PlaybackDetectService::PlaybackDetectService(QObject *parent)
    : QObject(parent)
    , m_detectionEngine(new DetectionEngine(this))
    , m_vehicleDetectEnabled(true)
    , m_trafficLightDetectEnabled(true)
{
    if (m_detectionEngine->isLoaded()) {
        qDebug() << "播放检测-检测引擎(YOLOv8n)初始化成功";
    } else {
        qWarning() << "播放检测-检测引擎(YOLOv8n)初始化失败";
    }
}

PlaybackDetectService::~PlaybackDetectService()
{
}

bool PlaybackDetectService::vehicleDetectEnabled() const { return m_vehicleDetectEnabled; }
bool PlaybackDetectService::trafficLightDetectEnabled() const { return m_trafficLightDetectEnabled; }

void PlaybackDetectService::setVehicleDetectEnabled(bool enabled)
{
    if (m_vehicleDetectEnabled != enabled) {
        m_vehicleDetectEnabled = enabled;
        emit vehicleDetectEnabledChanged();
    }
}

void PlaybackDetectService::setTrafficLightDetectEnabled(bool enabled)
{
    if (m_trafficLightDetectEnabled != enabled) {
        m_trafficLightDetectEnabled = enabled;
        emit trafficLightDetectEnabledChanged();
    }
}

QImage PlaybackDetectService::detectAndDrawFrame(const QImage &frame)
{
    if (frame.isNull()) return frame;

    // QImage → cv::Mat (BGR)
    QImage rgbImg = frame.convertToFormat(QImage::Format_RGB888);
    cv::Mat mat(rgbImg.height(), rgbImg.width(), CV_8UC3,
                const_cast<uchar*>(rgbImg.bits()), rgbImg.bytesPerLine());
    cv::Mat bgrMat;
    cv::cvtColor(mat, bgrMat, cv::COLOR_RGB2BGR);

    QVariantList vehicleResults;
    TrafficLightResult tlResult;

    if (m_detectionEngine->isLoaded()) {
        if (m_vehicleDetectEnabled && m_trafficLightDetectEnabled) {
            m_detectionEngine->detectAll(bgrMat, vehicleResults, tlResult);
        } else if (m_vehicleDetectEnabled) {
            m_detectionEngine->detectVehicles(bgrMat, vehicleResults);
        } else if (m_trafficLightDetectEnabled) {
            m_detectionEngine->detectTrafficLights(bgrMat, tlResult);
        }

        // 绘制检测框
        if (m_vehicleDetectEnabled || m_trafficLightDetectEnabled) {
            m_detectionEngine->drawDetections(bgrMat, vehicleResults,
                                               tlResult.trafficLightRects,
                                               tlResult.state,
                                               "360 Scene Replay");
        }
    }

    // cv::Mat (BGR) → QImage (RGB)
    cv::Mat rgbResult;
    cv::cvtColor(bgrMat, rgbResult, cv::COLOR_BGR2RGB);
    QImage result(rgbResult.data, rgbResult.cols, rgbResult.rows, rgbResult.step,
                  QImage::Format_RGB888);

    QString detectionInfo;
    if (!vehicleResults.isEmpty()) {
        detectionInfo += QString("vehicles:%1").arg(vehicleResults.size());
    }
    if (tlResult.state != "unknown") {
        if (!detectionInfo.isEmpty()) detectionInfo += ",";
        detectionInfo += QString("traffic_light:%1").arg(tlResult.state);
    }

    emit detectionResultReady(result.copy(), detectionInfo);
    return result.copy();
}

QString PlaybackDetectService::saveScreenshot(const QImage &frame, const QString &videoDir)
{
    if (frame.isNull()) return "";

    QString screenshotDir = videoDir + "/screenshots/";
    QDir dir(screenshotDir);
    if (!dir.exists()) dir.mkpath(".");

    QDateTime now = QDateTime::currentDateTime();
    QString fileName = "播放截图_" + now.toString("yyyyMMdd_HHmmss") + ".png";
    QString filePath = screenshotDir + fileName;

    if (frame.save(filePath, "PNG")) {
        qDebug() << "播放截图保存成功:" << filePath;
        return filePath;
    } else {
        qWarning() << "播放截图保存失败:" << filePath;
        return "";
    }
}
