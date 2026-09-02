#include "VideoRecordViewModel.h"
#include "services/network/FdbusClientService.h"
#include "services/auth/SessionContext.h"
#include <QDebug>

// ==================== VideoListModel ====================

VideoListModel::VideoListModel(QObject *parent)
    : QAbstractListModel(parent)
{
}

int VideoListModel::rowCount(const QModelIndex &parent) const
{
    Q_UNUSED(parent)
    return m_videos.count();
}

QVariant VideoListModel::data(const QModelIndex &index, int role) const
{
    if (!index.isValid() || index.row() >= m_videos.count())
        return QVariant();

    const VideoInfo &video = m_videos[index.row()];

    switch (role) {
    case VideoIdRole:         return video.id;
    case VideoNameRole:       return video.videoName;
    case VideoDateRole:       return video.videoDate;
    case VideoSizeRole:       return video.videoSize;
    case VideoSizeMBRole:     return video.videoSizeMB;
    case VideoDurationRole:   return video.videoDuration;
    case VideoDurationSecRole: return video.videoDurationSec;
    case ResolutionRole:      return video.resolution;
    case FpsRole:             return video.fps;
    case CameraRole:          return video.camera;
    case LastPlayTimeRole:    return video.lastPlayTime;
    case LastPlaySecRole:     return video.lastPlaySec;
    case PlayCountRole:       return video.playCount;
    case FilePathRole:        return video.filePath;
    case RecordTypeRole:      return video.recordType;
    default:                  return QVariant();
    }
}

QHash<int, QByteArray> VideoListModel::roleNames() const
{
    QHash<int, QByteArray> roles;
    roles[VideoIdRole]         = "videoId";
    roles[VideoNameRole]       = "videoName";
    roles[VideoDateRole]       = "videoDate";
    roles[VideoSizeRole]       = "videoSize";
    roles[VideoSizeMBRole]     = "videoSizeMB";
    roles[VideoDurationRole]   = "videoDuration";
    roles[VideoDurationSecRole] = "videoDurationSec";
    roles[ResolutionRole]      = "resolution";
    roles[FpsRole]             = "fps";
    roles[CameraRole]          = "camera";
    roles[LastPlayTimeRole]    = "lastPlayTime";
    roles[LastPlaySecRole]     = "lastPlaySec";
    roles[PlayCountRole]       = "playCount";
    roles[FilePathRole]        = "filePath";
    roles[RecordTypeRole]      = "recordType";
    return roles;
}

void VideoListModel::refresh(const QList<VideoInfo> &videos)
{
    beginResetModel();
    m_videos = videos;
    endResetModel();
}

void VideoListModel::updatePlayRecord(int index, int lastPlaySec, const QString &lastPlayTime)
{
    if (index < 0 || index >= m_videos.count())
        return;

    m_videos[index].lastPlaySec = lastPlaySec;
    m_videos[index].lastPlayTime = lastPlayTime;
    m_videos[index].playCount++;

    QModelIndex modelIndex = createIndex(index, 0);
    emit dataChanged(modelIndex, modelIndex);
}

// ==================== VideoRecordViewModel ====================

// FDBus消息ID（与服务端一致）
enum VideoMsgId {
    REQ_GET_VIDEO_LIST = 4,
    REQ_DELETE_VIDEO = 5,
    REQ_UPLOAD_VIDEO = 6,
    REQ_UPDATE_PLAY_RECORD = 8
};

VideoRecordViewModel::VideoRecordViewModel(QObject *parent)
    : QObject(parent)
    , m_videoListModel(new VideoListModel(this))
{
    // 连接FDBus信号
    connect(&FdbusClientService::instance(), &FdbusClientService::requestSuccess,
            this, &VideoRecordViewModel::onFdbusReply);
    connect(&FdbusClientService::instance(), &FdbusClientService::requestFailed,
            this, &VideoRecordViewModel::onFdbusError);
}

VideoListModel* VideoRecordViewModel::videoListModel() const
{
    return m_videoListModel;
}

int VideoRecordViewModel::videoCount() const
{
    return m_videoListModel->rowCount();
}

void VideoRecordViewModel::refreshVideoList()
{
    m_pendingRequestType = REQ_GET_VIDEO_LIST;
    m_pendingRequestId = FdbusClientService::instance().sendGetVideoListRequest(
        SessionContext::instance().username());
    if (m_pendingRequestId == 0) {
        emit videoListLoaded(false);
        m_pendingRequestType = 0;
    }
}

void VideoRecordViewModel::updatePlayRecord(int videoId, int lastPlaySec)
{
    m_pendingRequestType = REQ_UPDATE_PLAY_RECORD;
    m_pendingRequestId = FdbusClientService::instance().sendUpdatePlayRecordRequest(
        videoId, SessionContext::instance().username(), lastPlaySec);
    if (m_pendingRequestId == 0) {
        emit playRecordUpdated(false);
        m_pendingRequestType = 0;
    }
}

void VideoRecordViewModel::deleteVideo(int videoId)
{
    m_pendingRequestType = REQ_DELETE_VIDEO;
    m_pendingRequestId = FdbusClientService::instance().sendDeleteVideoRequest(
        videoId, SessionContext::instance().username());
    if (m_pendingRequestId == 0) {
        m_pendingRequestType = 0;
    }
}

void VideoRecordViewModel::uploadVideoRecord(const QString &videoName, int videoSizeMB,
                                               const QString &videoPath,
                                               const QString &recordType,
                                               int durationSec,
                                               const QString &resolution,
                                               int fps,
                                               const QString &cameraSource)
{
    m_pendingRequestType = REQ_UPLOAD_VIDEO;
    m_pendingRequestId = FdbusClientService::instance().sendUploadVideoRequest(
        SessionContext::instance().username(), videoName, videoSizeMB, videoPath, recordType,
        durationSec, resolution, fps, cameraSource);
    if (m_pendingRequestId == 0) {
        emit videoUploaded(false);
        m_pendingRequestType = 0;
    }
}

QString VideoRecordViewModel::formatSeconds(int sec)
{
    int h = sec / 3600;
    int m = (sec % 3600) / 60;
    int s = sec % 60;
    return QString("%1:%2:%3")
        .arg(h, 2, 10, QChar('0'))
        .arg(m, 2, 10, QChar('0'))
        .arg(s, 2, 10, QChar('0'));
}

void VideoRecordViewModel::onFdbusReply(qint64 requestId, const QJsonObject &data)
{
    // 仅处理自己发起的请求，避免被其他 ViewModel 的响应串扰
    if (requestId != m_pendingRequestId) return;

    switch (m_pendingRequestType) {
    case REQ_GET_VIDEO_LIST:
        parseVideoListResponse(data);
        break;
    case REQ_UPDATE_PLAY_RECORD:
        emit playRecordUpdated(data.value("success").toBool());
        // 播放记录更新后刷新列表
        refreshVideoList();
        break;
    case REQ_UPLOAD_VIDEO:
        emit videoUploaded(data.value("success").toBool());
        // 上传成功后刷新列表
        if (data.value("success").toBool()) {
            refreshVideoList();
        }
        break;
    case REQ_DELETE_VIDEO:
        if (data.value("success").toBool()) {
            refreshVideoList();
        }
        break;
    }

    m_pendingRequestId = 0;
    m_pendingRequestType = 0;
}

void VideoRecordViewModel::onFdbusError(qint64 requestId, const QString &error)
{
    if (requestId != m_pendingRequestId) return;

    qWarning() << "视频记录请求失败:" << error;

    switch (m_pendingRequestType) {
    case REQ_GET_VIDEO_LIST:
        emit videoListLoaded(false);
        break;
    case REQ_UPDATE_PLAY_RECORD:
        emit playRecordUpdated(false);
        break;
    case REQ_UPLOAD_VIDEO:
        emit videoUploaded(false);
        break;
    }

    m_pendingRequestId = 0;
    m_pendingRequestType = 0;
}

void VideoRecordViewModel::parseVideoListResponse(const QJsonObject &data)
{
    QList<VideoInfo> videos;

    if (!data.value("success").toBool()) {
        emit videoListLoaded(false);
        return;
    }

    QJsonArray records = data.value("records").toArray();

    for (const QJsonValue &val : records) {
        QJsonObject rec = val.toObject();
        VideoInfo info;
        info.id = rec.value("id").toInt();
        info.videoName = rec.value("video_name").toString();
        info.videoDate = rec.value("created_at").toString();
        info.videoSizeMB = rec.value("video_size").toInt();
        info.videoSize = QString::number(info.videoSizeMB) + "MB";
        info.videoDurationSec = rec.value("duration_sec").toInt();
        info.videoDuration = formatSeconds(info.videoDurationSec);
        info.resolution = rec.value("resolution").toString();
        info.fps = rec.value("fps").toInt();
        info.camera = rec.value("camera_source").toString();
        info.lastPlaySec = rec.value("last_play_sec").toInt();
        info.lastPlayTime = info.lastPlaySec > 0 ? formatSeconds(info.lastPlaySec) : "";
        info.playCount = rec.value("play_count").toInt();
        info.filePath = rec.value("video_path").toString();
        info.recordType = rec.value("record_type").toString();
        videos.append(info);
    }

    m_videoListModel->refresh(videos);
    emit videoCountChanged();
    emit videoListLoaded(true);
}
