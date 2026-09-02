#ifndef BIRDRECORDVIEWMODEL_H
#define BIRDRECORDVIEWMODEL_H

#include <QObject>
#include <QString>

// 前向声明
class VideoFrameProvider;

// 鸟瞰模式视图模型 — 代理 BirdRecordService 状态属性
// MVVM: ViewModel 在主线程持有 Service, 转发 start/stop + 状态信号
class BirdRecordViewModel : public QObject
{
    Q_OBJECT

    Q_PROPERTY(bool isRunning READ isRunning NOTIFY runningChanged)
    Q_PROPERTY(bool imagesLoaded READ imagesLoaded NOTIFY imagesLoadedChanged)
    Q_PROPERTY(int frameCounter READ frameCounter NOTIFY frameCounterChanged)
    Q_PROPERTY(bool isRecording READ isRecording NOTIFY recordingStateChanged)
    Q_PROPERTY(QString recordFileName READ recordFileName NOTIFY recordFileNameChanged)
    Q_PROPERTY(int recordDuration READ recordDuration NOTIFY recordDurationChanged)
    Q_PROPERTY(int writtenFrames READ writtenFrames NOTIFY writtenFramesChanged)
    Q_PROPERTY(QString videoDir READ videoDir NOTIFY videoDirChanged)

public:
    explicit BirdRecordViewModel(VideoFrameProvider *provider = nullptr, QObject *parent = nullptr);
    ~BirdRecordViewModel();

    bool isRunning() const;
    bool imagesLoaded() const;
    int frameCounter() const;
    bool isRecording() const;
    QString recordFileName() const;
    int recordDuration() const;
    int writtenFrames() const;
    QString videoDir() const;

    Q_INVOKABLE bool start();
    Q_INVOKABLE void stop();

signals:
    void runningChanged();
    void imagesLoadedChanged();
    void frameCounterChanged();
    void recordingStateChanged();
    void recordFileNameChanged();
    void recordDurationChanged();
    void writtenFramesChanged();
    void videoDirChanged();
    void buildFailed(const QString &reason);

private:
    class BirdRecordService *m_service;
};

#endif // BIRDRECORDVIEWMODEL_H
