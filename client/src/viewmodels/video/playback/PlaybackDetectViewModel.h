#ifndef PLAYBACKDETECTVIEWMODEL_H
#define PLAYBACKDETECTVIEWMODEL_H

#include <QObject>
#include <QString>
#include <QImage>
#include <QJsonObject>

// 播放检测视图模型 - 连接PlaybackDetectService和ScreenshotViewModel
class PlaybackDetectViewModel : public QObject
{
    Q_OBJECT

    Q_PROPERTY(bool isDetecting READ isDetecting NOTIFY detectingChanged)
    Q_PROPERTY(QString lastScreenshotPath READ lastScreenshotPath NOTIFY lastScreenshotPathChanged)

public:
    explicit PlaybackDetectViewModel(QObject *parent = nullptr);

    bool isDetecting() const;
    QString lastScreenshotPath() const;

    // QML可调用：对帧执行检测
    Q_INVOKABLE QImage requestDetection(const QImage &frame);

    // QML可调用：截取当前帧保存并上传
    Q_INVOKABLE void takeScreenshot(const QImage &frame, const QString &videoDir);

signals:
    void detectingChanged();
    void lastScreenshotPathChanged();
    void frameAnnotated(const QImage &annotatedFrame);
    void screenshotTaken(const QString &filePath);

private slots:
    void onFdbusReply(qint64 requestId, const QJsonObject &data);
    void onFdbusError(qint64 requestId, const QString &error);

private:
    bool m_isDetecting = false;
    QString m_lastScreenshotPath;
    qint64 m_pendingRequestId = 0;  // 请求-响应关联令牌
    QString m_pendingFilePath;
};

#endif // PLAYBACKDETECTVIEWMODEL_H
