#ifndef REVERSERECORDVIEWMODEL_H
#define REVERSERECORDVIEWMODEL_H

#include <QObject>
#include <QString>

// 倒车模式视图模型 - 在主线程持有ReverseRecordService，属性代理Service状态
class ReverseRecordViewModel : public QObject
{
    Q_OBJECT

    Q_PROPERTY(bool isAutoRecording READ isAutoRecording NOTIFY autoRecordingChanged)
    Q_PROPERTY(bool isAutoRecordEnabled READ isAutoRecordEnabled WRITE setAutoRecordEnabled NOTIFY autoRecordEnabledChanged)
    Q_PROPERTY(int autoRecordIntervalSec READ autoRecordIntervalSec WRITE setAutoRecordIntervalSec NOTIFY autoRecordIntervalChanged)
    Q_PROPERTY(int targetFrames READ targetFrames WRITE setTargetFrames NOTIFY targetFramesChanged)
    Q_PROPERTY(int currentFrameCount READ currentFrameCount NOTIFY currentFrameCountChanged)
    Q_PROPERTY(bool isRunning READ isRunning NOTIFY runningChanged)
    Q_PROPERTY(QString videoDir READ videoDir WRITE setVideoDir NOTIFY videoDirChanged)
    Q_PROPERTY(qreal steeringAngle READ steeringAngle NOTIFY steeringAngleChanged)
    Q_PROPERTY(bool useStaticImage READ useStaticImage NOTIFY useStaticImageChanged)
    Q_PROPERTY(QString staticImagePath READ staticImagePath NOTIFY staticImageChanged)

public:
    explicit ReverseRecordViewModel(QObject *parent = nullptr);
    ~ReverseRecordViewModel();

    bool isAutoRecording() const;
    bool isAutoRecordEnabled() const;
    void setAutoRecordEnabled(bool enabled);
    int autoRecordIntervalSec() const;
    void setAutoRecordIntervalSec(int sec);
    int targetFrames() const;
    void setTargetFrames(int frames);
    int currentFrameCount() const;
    bool isRunning() const;
    QString videoDir() const;
    void setVideoDir(const QString &dir);
    qreal steeringAngle() const;
    bool useStaticImage() const;
    QString staticImagePath() const;

    Q_INVOKABLE bool start();
    Q_INVOKABLE void stop();
    Q_INVOKABLE bool openCamera(int camId = 0);
    Q_INVOKABLE void closeCamera();
    Q_INVOKABLE void steerLeft();
    Q_INVOKABLE void steerRight();
    Q_INVOKABLE void steerCenter();
    Q_INVOKABLE bool loadStaticImage(const QString &path);

signals:
    void autoRecordingChanged();
    void autoRecordEnabledChanged();
    void autoRecordIntervalChanged();
    void targetFramesChanged();
    void currentFrameCountChanged();
    void runningChanged();
    void videoDirChanged();
    void steeringAngleChanged();
    void useStaticImageChanged();
    void staticImageChanged();
    void recordingSaved(const QString &filePath, int frames);

private:
    class ReverseRecordService *m_service;
};

#endif // REVERSERECORDVIEWMODEL_H
