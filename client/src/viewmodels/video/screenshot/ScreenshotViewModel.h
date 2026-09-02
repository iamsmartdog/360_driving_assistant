#ifndef SCREENSHOTVIEWMODEL_H
#define SCREENSHOTVIEWMODEL_H

#include <QObject>
#include <QString>
#include <QJsonObject>

// 截图上传视图模型 - 接收Service的截图信号，通过FDBus上传截图元数据到服务器
class ScreenshotViewModel : public QObject
{
    Q_OBJECT

    Q_PROPERTY(int uploadCount READ uploadCount NOTIFY uploadCountChanged)

public:
    explicit ScreenshotViewModel(QObject *parent = nullptr);

    int uploadCount() const;

    // QML可调用方法：上传截图元数据到服务器
    Q_INVOKABLE void uploadScreenshot(const QString &filePath,
                                       const QString &detectionInfo,
                                       const QString &recordType = "行车截图");

signals:
    void uploadCountChanged();
    void screenshotUploaded(bool success, const QString &filePath);

private slots:
    void onFdbusReply(qint64 requestId, const QJsonObject &data);
    void onFdbusError(qint64 requestId, const QString &error);

private:
    int m_uploadCount = 0;
    qint64 m_pendingRequestId = 0;  // 请求-响应关联令牌
    QString m_pendingFilePath;
};

#endif // SCREENSHOTVIEWMODEL_H
