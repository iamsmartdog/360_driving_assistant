#ifndef SCREENSHOTLISTVIEWMODEL_H
#define SCREENSHOTLISTVIEWMODEL_H

#include <QObject>
#include <QString>
#include <QAbstractListModel>
#include <QJsonArray>
#include <QJsonObject>

// 截图记录数据结构
struct ScreenshotInfo {
    int id = 0;
    QString screenshotName;
    int screenshotSizeKB = 0;
    QString screenshotPath;
    QString recordType;       // 行车截图/播放截图/倒车截图
    QString detectionInfo;    // 检测结果JSON
    QString resolution;
    QString cameraSource;
    QString createdAt;
};

// 截图列表模型 - 供QML ListView使用
class ScreenshotListModel : public QAbstractListModel
{
    Q_OBJECT

public:
    enum Roles {
        IdRole = Qt::UserRole + 1,
        ScreenshotNameRole,
        ScreenshotSizeRole,
        ScreenshotPathRole,
        RecordTypeRole,
        DetectionInfoRole,
        ResolutionRole,
        CameraSourceRole,
        CreatedAtRole
    };

    explicit ScreenshotListModel(QObject *parent = nullptr);

    int rowCount(const QModelIndex &parent = QModelIndex()) const override;
    QVariant data(const QModelIndex &index, int role) const override;
    QHash<int, QByteArray> roleNames() const override;

    void refresh(const QList<ScreenshotInfo> &screenshots);

private:
    QList<ScreenshotInfo> m_screenshots;
};

// 截图列表视图模型 - 通过FDBus从服务端获取截图数据
class ScreenshotListViewModel : public QObject
{
    Q_OBJECT

    Q_PROPERTY(ScreenshotListModel* screenshotListModel READ screenshotListModel CONSTANT)
    Q_PROPERTY(int screenshotCount READ screenshotCount NOTIFY screenshotCountChanged)

public:
    explicit ScreenshotListViewModel(QObject *parent = nullptr);

    ScreenshotListModel* screenshotListModel() const;
    int screenshotCount() const;

    Q_INVOKABLE void refreshScreenshotList();
    Q_INVOKABLE void deleteScreenshot(int screenshotId);

signals:
    void screenshotCountChanged();
    void screenshotListLoaded(bool success);
    void screenshotDeleted(bool success);

private slots:
    void onFdbusReply(qint64 requestId, const QJsonObject &data);
    void onFdbusError(qint64 requestId, const QString &error);

private:
    void parseScreenshotListResponse(const QJsonObject &data);

    ScreenshotListModel *m_screenshotListModel;
    qint64 m_pendingRequestId = 0;  // 请求-响应关联令牌
};

#endif // SCREENSHOTLISTVIEWMODEL_H
