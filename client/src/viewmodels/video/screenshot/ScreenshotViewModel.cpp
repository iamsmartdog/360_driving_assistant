#include "ScreenshotViewModel.h"
#include "services/network/FdbusClientService.h"
#include "services/auth/SessionContext.h"
#include <QFileInfo>
#include <QDebug>

ScreenshotViewModel::ScreenshotViewModel(QObject *parent)
    : QObject(parent)
{
    // 连接FDBus信号
    connect(&FdbusClientService::instance(), &FdbusClientService::requestSuccess,
            this, &ScreenshotViewModel::onFdbusReply);
    connect(&FdbusClientService::instance(), &FdbusClientService::requestFailed,
            this, &ScreenshotViewModel::onFdbusError);
}

int ScreenshotViewModel::uploadCount() const { return m_uploadCount; }

void ScreenshotViewModel::uploadScreenshot(const QString &filePath,
                                            const QString &detectionInfo,
                                            const QString &recordType)
{
    QFileInfo fi(filePath);
    if (!fi.exists()) {
        qWarning() << "截图文件不存在:" << filePath;
        emit screenshotUploaded(false, filePath);
        return;
    }

    int sizeKB = static_cast<int>(fi.size() / 1024);
    QString fileName = fi.fileName();

    m_pendingFilePath = filePath;

    m_pendingRequestId = FdbusClientService::instance().sendUploadScreenshotRequest(
        SessionContext::instance().username(),
        fileName, sizeKB, filePath, recordType, detectionInfo
    );
    if (m_pendingRequestId == 0) {
        emit screenshotUploaded(false, filePath);
        m_pendingFilePath.clear();
        return;
    }

    qDebug() << "上传截图元数据:" << fileName << "大小:" << sizeKB << "KB"
             << "类型:" << recordType << "检测:" << detectionInfo;
}

void ScreenshotViewModel::onFdbusReply(qint64 requestId, const QJsonObject &data)
{
    if (requestId != m_pendingRequestId) return;

    bool success = data["success"].toBool();
    if (success) {
        m_uploadCount++;
        emit uploadCountChanged();
    }
    emit screenshotUploaded(success, m_pendingFilePath);
    m_pendingRequestId = 0;
    m_pendingFilePath.clear();
}

void ScreenshotViewModel::onFdbusError(qint64 requestId, const QString &error)
{
    if (requestId != m_pendingRequestId) return;

    qWarning() << "截图上传失败:" << error;
    emit screenshotUploaded(false, m_pendingFilePath);
    m_pendingRequestId = 0;
    m_pendingFilePath.clear();
}
