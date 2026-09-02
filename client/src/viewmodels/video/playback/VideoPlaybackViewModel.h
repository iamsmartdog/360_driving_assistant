#ifndef VIDEOPLAYBACKVIEWMODEL_H
#define VIDEOPLAYBACKVIEWMODEL_H

#include <QObject>
#include <QString>
#include <QImage>
#include <QJsonObject>

// 视频播放视图模型 - 连接VideoPlaybackService和截图上传
class VideoFrameProvider;

class VideoPlaybackViewModel : public QObject
{
    Q_OBJECT

    Q_PROPERTY(bool isPlaying READ isPlaying NOTIFY playingChanged)
    Q_PROPERTY(int currentPositionSec READ currentPositionSec NOTIFY positionChanged)
    Q_PROPERTY(int totalDurationSec READ totalDurationSec NOTIFY durationChanged)
    Q_PROPERTY(int currentPositionMs READ currentPositionMs NOTIFY positionMsChanged)
    Q_PROPERTY(int totalDurationMs READ totalDurationMs NOTIFY videoInfoChanged)
    Q_PROPERTY(int currentFrame READ currentFrame NOTIFY frameChanged)
    Q_PROPERTY(int totalFrames READ totalFrames NOTIFY videoInfoChanged)
    Q_PROPERTY(double fps READ fps NOTIFY videoInfoChanged)
    Q_PROPERTY(bool videoLoaded READ videoLoaded NOTIFY videoInfoChanged)
    Q_PROPERTY(double playbackSpeed READ playbackSpeed WRITE setPlaybackSpeed NOTIFY speedChanged)
    Q_PROPERTY(bool vehicleDetectEnabled READ vehicleDetectEnabled WRITE setVehicleDetectEnabled NOTIFY vehicleDetectEnabledChanged)
    Q_PROPERTY(bool trafficLightDetectEnabled READ trafficLightDetectEnabled WRITE setTrafficLightDetectEnabled NOTIFY trafficLightDetectEnabledChanged)
    Q_PROPERTY(QString screenshotMessage READ screenshotMessage NOTIFY screenshotMessageChanged)
    Q_PROPERTY(int frameCounter READ frameCounter NOTIFY frameCounterChanged)

public:
    explicit VideoPlaybackViewModel(VideoFrameProvider *provider, QObject *parent = nullptr);
    ~VideoPlaybackViewModel();

    bool isPlaying() const;
    int currentPositionSec() const;
    int totalDurationSec() const;
    int currentPositionMs() const;
    int totalDurationMs() const;
    int currentFrame() const;
    int totalFrames() const;
    double fps() const;
    bool videoLoaded() const;
    double playbackSpeed() const;
    void setPlaybackSpeed(double speed);
    bool vehicleDetectEnabled() const;
    void setVehicleDetectEnabled(bool enabled);
    bool trafficLightDetectEnabled() const;
    void setTrafficLightDetectEnabled(bool enabled);
    QString screenshotMessage() const;
    int frameCounter() const;

    Q_INVOKABLE bool openVideo(const QString &filePath, int resumeSec = 0);
    Q_INVOKABLE void closeVideo();
    Q_INVOKABLE void togglePlayPause();
    Q_INVOKABLE void seekToSec(double sec);
    Q_INVOKABLE void takeScreenshotAndUpload();
    Q_INVOKABLE void updatePlayRecord(int videoId, int lastPlaySec);

signals:
    void playingChanged();
    void positionChanged();
    void durationChanged();
    void positionMsChanged();
    void frameChanged();
    void videoInfoChanged();
    void speedChanged();
    void vehicleDetectEnabledChanged();
    void trafficLightDetectEnabledChanged();
    void screenshotMessageChanged();
    void frameCounterChanged();
    void newFrameReady(const QImage &frame);
    void playbackFinished();
    void playRecordUpdated(bool success);
    void screenshotUploaded(bool success, const QString &message);

private slots:
    void onFrameReady(const QImage &frame);
    void onPlaybackFinished();
    void onScreenshotSaved(const QString &filePath);
    void onFdbusReply(qint64 requestId, const QJsonObject &data);
    void onFdbusError(qint64 requestId, const QString &error);

private:
    class VideoPlaybackService *m_service;
    QString m_screenshotMessage;
    qint64 m_pendingRequestId = 0;  // 请求-响应关联令牌
    int m_pendingRequestType = 0;   // 当前等待回复的请求类型（本地派发用）
    QString m_pendingFilePath;
    int m_currentVideoId = 0;
};

#endif // VIDEOPLAYBACKVIEWMODEL_H
