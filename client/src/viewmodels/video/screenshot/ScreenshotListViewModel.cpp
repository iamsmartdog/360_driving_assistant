#include "ScreenshotListViewModel.h"
#include "services/network/FdbusClientService.h"
#include "services/auth/SessionContext.h"
#include <QDebug>

// ==================== ScreenshotListModel ====================

ScreenshotListModel::ScreenshotListModel(QObject *parent)
    : QAbstractListModel(parent)
{
}

int ScreenshotListModel::rowCount(const QModelIndex &parent) const
{
    Q_UNUSED(parent)
    return m_screenshots.count();
}

QVariant ScreenshotListModel::data(const QModelIndex &index, int role) const
{
    if (!index.isValid() || index.row() >= m_screenshots.count())
        return QVariant();

    const ScreenshotInfo &item = m_screenshots[index.row()];

    switch (role) {
    case IdRole:              return item.id;
    case ScreenshotNameRole:  return item.screenshotName;
    case ScreenshotSizeRole:  return item.screenshotSizeKB;
    case ScreenshotPathRole:  return item.screenshotPath;
    case RecordTypeRole:      return item.recordType;
    case DetectionInfoRole:   return item.detectionInfo;
    case ResolutionRole:      return item.resolution;
    case CameraSourceRole:    return item.cameraSource;
    case CreatedAtRole:       return item.createdAt;
    default:                  return QVariant();
    }
}

QHash<int, QByteArray> ScreenshotListModel::roleNames() const
{
    QHash<int, QByteArray> roles;
    roles[IdRole]             = "screenshotId";
    roles[ScreenshotNameRole] = "screenshotName";
    roles[ScreenshotSizeRole] = "screenshotSizeKB";
    roles[ScreenshotPathRole] = "screenshotPath";
    roles[RecordTypeRole]     = "recordType";
    roles[DetectionInfoRole]  = "detectionInfo";
    roles[ResolutionRole]     = "resolution";
    roles[CameraSourceRole]   = "cameraSource";
    roles[CreatedAtRole]      = "createdAt";
    return roles;
}

void ScreenshotListModel::refresh(const QList<ScreenshotInfo> &screenshots)
{
    beginResetModel();
    m_screenshots = screenshots;
    endResetModel();
}

// ==================== ScreenshotListViewModel ====================

ScreenshotListViewModel::ScreenshotListViewModel(QObject *parent)
    : QObject(parent)
    , m_screenshotListModel(new ScreenshotListModel(this))
{
    connect(&FdbusClientService::instance(), &FdbusClientService::requestSuccess,
            this, &ScreenshotListViewModel::onFdbusReply);
    connect(&FdbusClientService::instance(), &FdbusClientService::requestFailed,
            this, &ScreenshotListViewModel::onFdbusError);
}

ScreenshotListModel* ScreenshotListViewModel::screenshotListModel() const
{
    return m_screenshotListModel;
}

int ScreenshotListViewModel::screenshotCount() const
{
    return m_screenshotListModel->rowCount();
}

void ScreenshotListViewModel::refreshScreenshotList()
{
    m_pendingRequestId = FdbusClientService::instance().sendGetScreenshotListRequest(
        SessionContext::instance().username());
    if (m_pendingRequestId == 0) {
        emit screenshotListLoaded(false);
    }
}

void ScreenshotListViewModel::deleteScreenshot(int screenshotId)
{
    Q_UNUSED(screenshotId)
    // 目前服务端没有删除截图的接口，预留
    qDebug() << "删除截图功能待实现";
}

void ScreenshotListViewModel::onFdbusReply(qint64 requestId, const QJsonObject &data)
{
    if (requestId != m_pendingRequestId) return;
    parseScreenshotListResponse(data);
    m_pendingRequestId = 0;
}

void ScreenshotListViewModel::onFdbusError(qint64 requestId, const QString &error)
{
    if (requestId != m_pendingRequestId) return;
    qWarning() << "截图列表请求失败:" << error;
    emit screenshotListLoaded(false);
    m_pendingRequestId = 0;
}

void ScreenshotListViewModel::parseScreenshotListResponse(const QJsonObject &data)
{
    QList<ScreenshotInfo> screenshots;

    if (!data.value("success").toBool()) {
        emit screenshotListLoaded(false);
        return;
    }

    QJsonArray records = data.value("records").toArray();

    for (const QJsonValue &val : records) {
        QJsonObject rec = val.toObject();
        ScreenshotInfo info;
        info.id = rec.value("id").toInt();
        info.screenshotName = rec.value("screenshot_name").toString();
        info.screenshotSizeKB = rec.value("screenshot_size").toInt();
        info.screenshotPath = rec.value("screenshot_path").toString();
        info.recordType = rec.value("record_type").toString();
        info.detectionInfo = rec.value("detection_info").toString();
        info.resolution = rec.value("resolution").toString();
        info.cameraSource = rec.value("camera_source").toString();
        info.createdAt = rec.value("created_at").toString();
        screenshots.append(info);
    }

    m_screenshotListModel->refresh(screenshots);
    emit screenshotCountChanged();
    emit screenshotListLoaded(true);
}
