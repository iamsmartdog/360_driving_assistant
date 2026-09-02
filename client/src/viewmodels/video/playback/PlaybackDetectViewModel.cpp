#include "PlaybackDetectViewModel.h"
#include "services/video/playback/PlaybackDetectService.h"
#include "services/network/FdbusClientService.h"
#include "services/auth/SessionContext.h"
#include "viewmodels/video/screenshot/ScreenshotViewModel.h"
#include <QFileInfo>
#include <QDebug>

PlaybackDetectViewModel::PlaybackDetectViewModel(QObject *parent)
    : QObject(parent)
{
    connect(&FdbusClientService::instance(), &FdbusClientService::requestSuccess,
            this, &PlaybackDetectViewModel::onFdbusReply);
    connect(&FdbusClientService::instance(), &FdbusClientService::requestFailed,
            this, &PlaybackDetectViewModel::onFdbusError);
}

bool PlaybackDetectViewModel::isDetecting() const { return m_isDetecting; }
QString PlaybackDetectViewModel::lastScreenshotPath() const { return m_lastScreenshotPath; }

QImage PlaybackDetectViewModel::requestDetection(const QImage &frame)
{
    if (frame.isNull()) return frame;

    m_isDetecting = true;
    emit detectingChanged();

    // 使用PlaybackDetectService执行检测
    static PlaybackDetectService service;
    QImage annotatedFrame = service.detectAndDrawFrame(frame);

    m_isDetecting = false;
    emit detectingChanged();

    emit frameAnnotated(annotatedFrame);
    return annotatedFrame;
}

void PlaybackDetectViewModel::takeScreenshot(const QImage &frame, const QString &videoDir)
{
    if (frame.isNull()) return;

    // 先执行检测绘制框
    static PlaybackDetectService service;
    QImage annotatedFrame = service.detectAndDrawFrame(frame);

    // 保存截图
    QString filePath = service.saveScreenshot(annotatedFrame, videoDir);
    if (filePath.isEmpty()) return;

    m_lastScreenshotPath = filePath;
    emit lastScreenshotPathChanged();
    emit screenshotTaken(filePath);

    // 获取检测信息
    QString detectionInfo;
    if (service.vehicleDetectEnabled()) {
        detectionInfo += "vehicle_detection:on";
    }
    if (service.trafficLightDetectEnabled()) {
        if (!detectionInfo.isEmpty()) detectionInfo += ",";
        detectionInfo += "traffic_light_detection:on";
    }

    // 上传截图元数据到服务器
    QFileInfo fi(filePath);
    int sizeKB = static_cast<int>(fi.size() / 1024);

    m_pendingFilePath = filePath;

    m_pendingRequestId = FdbusClientService::instance().sendUploadScreenshotRequest(
        SessionContext::instance().username(),
        fi.fileName(), sizeKB, filePath, "播放截图", detectionInfo
    );
    if (m_pendingRequestId == 0) {
        m_pendingFilePath.clear();
        return;
    }

    qDebug() << "播放截图上传:" << fi.fileName() << "大小:" << sizeKB << "KB";
}

void PlaybackDetectViewModel::onFdbusReply(qint64 requestId, const QJsonObject &data)
{
    if (requestId != m_pendingRequestId) return;
    bool success = data["success"].toBool();
    qDebug() << "播放截图上传" << (success ? "成功" : "失败");
    m_pendingRequestId = 0;
    m_pendingFilePath.clear();
}

void PlaybackDetectViewModel::onFdbusError(qint64 requestId, const QString &error)
{
    if (requestId != m_pendingRequestId) return;
    qWarning() << "播放截图上传失败:" << error;
    m_pendingRequestId = 0;
    m_pendingFilePath.clear();
}
