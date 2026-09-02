#ifndef VIDEORECORDVIEWMODEL_H
#define VIDEORECORDVIEWMODEL_H

#include <QObject>
#include <QString>
#include <QAbstractListModel>
#include <QDateTime>
#include <QJsonArray>
#include <QJsonObject>

// 单个视频项的数据结构
struct VideoInfo {
    int id = 0;                    // 数据库ID
    QString videoName;             // 视频名称
    QString videoDate;             // 录制时间
    QString videoSize;             // 文件大小（格式化字符串）
    int videoSizeMB = 0;           // 文件大小（MB）
    QString videoDuration;         // 视频时长（格式化字符串）
    int videoDurationSec = 0;      // 视频时长（秒）
    QString resolution;            // 分辨率
    int fps = 0;                   // 帧率
    QString camera;                // 摄像头来源
    QString lastPlayTime;          // 上次播放到的时间点
    int lastPlaySec = 0;           // 上次播放到的秒数
    int playCount = 0;             // 播放次数
    QString filePath;              // 文件路径
    QString recordType;            // 记录类型
};

// 视频列表模型 - 供QML ListView使用
class VideoListModel : public QAbstractListModel
{
    Q_OBJECT

public:
    enum Roles {
        VideoIdRole = Qt::UserRole + 1,
        VideoNameRole,
        VideoDateRole,
        VideoSizeRole,
        VideoSizeMBRole,
        VideoDurationRole,
        VideoDurationSecRole,
        ResolutionRole,
        FpsRole,
        CameraRole,
        LastPlayTimeRole,
        LastPlaySecRole,
        PlayCountRole,
        FilePathRole,
        RecordTypeRole
    };

    explicit VideoListModel(QObject *parent = nullptr);

    int rowCount(const QModelIndex &parent = QModelIndex()) const override;
    QVariant data(const QModelIndex &index, int role) const override;
    QHash<int, QByteArray> roleNames() const override;

    void refresh(const QList<VideoInfo> &videos);
    void updatePlayRecord(int index, int lastPlaySec, const QString &lastPlayTime);

private:
    QList<VideoInfo> m_videos;
};

// 视频记录视图模型 - 通过FDBus从服务端获取数据
class VideoRecordViewModel : public QObject
{
    Q_OBJECT

    Q_PROPERTY(VideoListModel* videoListModel READ videoListModel CONSTANT)
    Q_PROPERTY(int videoCount READ videoCount NOTIFY videoCountChanged)

public:
    explicit VideoRecordViewModel(QObject *parent = nullptr);

    VideoListModel* videoListModel() const;
    int videoCount() const;

    // QML可调用方法
    Q_INVOKABLE void refreshVideoList();
    Q_INVOKABLE void updatePlayRecord(int videoId, int lastPlaySec);
    Q_INVOKABLE void deleteVideo(int videoId);
    Q_INVOKABLE void uploadVideoRecord(const QString &videoName, int videoSizeMB,
                                        const QString &videoPath = "",
                                        const QString &recordType = "",
                                        int durationSec = 0,
                                        const QString &resolution = "",
                                        int fps = 0,
                                        const QString &cameraSource = "");
    Q_INVOKABLE QString formatSeconds(int sec);

signals:
    void videoCountChanged();
    void videoListLoaded(bool success);
    void playRecordUpdated(bool success);
    void videoUploaded(bool success);

private slots:
    void onFdbusReply(qint64 requestId, const QJsonObject &data);
    void onFdbusError(qint64 requestId, const QString &error);

private:
    void parseVideoListResponse(const QJsonObject &data);

    VideoListModel *m_videoListModel;
    qint64 m_pendingRequestId = 0;  // 请求-响应关联令牌
    int m_pendingRequestType = 0;   // 当前等待回复的请求类型（本地派发用）
};

#endif // VIDEORECORDVIEWMODEL_H
