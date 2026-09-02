#ifndef FDBUSCLIENTSERVICE_H
#define FDBUSCLIENTSERVICE_H

#include <QObject>
#include <QJsonObject>
#include <QJsonArray>
#include <QString>
#include <QMutex>
#include <QHash>
#include <QList>
#include <QtGlobal>

// FDBus前向声明
namespace ipc { namespace fdbus {
class CBaseWorker;
class CBaseJob;
}}

/**
 * @brief FDBus客户端服务（单例）
 * 管理与服务器端的FDBus连接。
 * 客户端和服务端在同一台机器上运行，通过name_server自动发现服务端。
 *
 * 请求-响应关联：每个 sendXxxRequest 返回一个全局唯一的 requestId（>0），
 * 响应通过 requestSuccess(requestId, ...) / requestFailed(requestId, ...) 回传。
 * 调用方用自己的 m_pendingRequestId 比对 requestId 来确认是否是自己的响应，
 * 彻底消除原先"广播信号 + m_pendingRequestType"在多 ViewModel 监听同一消息ID时
 * 的串扰问题（例如三个 ViewModel 同时监听 REQ_UPLOAD_SCREENSHOT=9）。
 *
 * 内部路由：按 msgCode 维护 FIFO 待响应队列。服务端单 worker 串行处理，
 * 同一 msgCode 的响应按请求顺序返回，队首即对应本次响应的发起者。
 */
class FdbusClientService : public QObject
{
    Q_OBJECT

public:
    // 单例访问
    static FdbusClientService& instance();

    // 通过name_server连接（本机自动发现）
    // serverName 为空时从 config.ini [FDBus] ServerName 读取
    bool connectByName(const QString &serverName = QString());

    // 断开连接（断开后可重新连接）
    void disconnectFromServer();

    // 是否已连接
    bool isConnected() const;

    // ===== 各请求：返回 requestId（>0 成功发出；0 表示未连接，调用方应本地处理失败）=====
    qint64 sendLoginRequest(const QString &username, const QString &password);
    qint64 sendRegisterRequest(const QString &username, const QString &password, const QString &nickname);
    qint64 sendHeartbeat();
    qint64 sendGetVideoListRequest(const QString &username = "", int page = 1, int pageSize = 20);
    qint64 sendUploadVideoRequest(const QString &username, const QString &videoName,
                                   int videoSize, const QString &videoPath = "",
                                   const QString &recordType = "",
                                   int durationSec = 0, const QString &resolution = "",
                                   int fps = 0, const QString &cameraSource = "");
    qint64 sendUpdatePlayRecordRequest(int videoId, const QString &username, int lastPlaySec);
    qint64 sendDeleteVideoRequest(int videoId, const QString &username);
    qint64 sendUploadScreenshotRequest(const QString &username, const QString &screenshotName,
                                        int screenshotSizeKB, const QString &screenshotPath = "",
                                        const QString &recordType = "",
                                        const QString &detectionInfo = "",
                                        const QString &resolution = "",
                                        const QString &cameraSource = "");
    qint64 sendGetScreenshotListRequest(const QString &username = "",
                                         const QString &recordType = "",
                                         int page = 1, int pageSize = 20);

signals:
    // 响应携带 requestId，调用方据此匹配自己的请求
    void requestSuccess(qint64 requestId, const QJsonObject &responseData);
    void requestFailed(qint64 requestId, const QString &errorMessage);
    void serverOnline();
    void serverOffline();

private:
    // 私有构造（单例）
    explicit FdbusClientService(QObject *parent = nullptr);
    ~FdbusClientService();

    // 禁止拷贝
    FdbusClientService(const FdbusClientService&) = delete;
    FdbusClientService& operator=(const FdbusClientService&) = delete;

    // 内部FDBus客户端实现（Pimpl模式）
    class FdbusClientImpl;
    FdbusClientImpl *m_impl = nullptr;

    bool m_isConnected = false;
    QMutex m_mutex;

    // 请求-响应关联：按 msgCode 维护待响应 requestId 的 FIFO 队列
    qint64 m_nextRequestId = 1;
    QHash<int, QList<qint64>> m_pendingByCode;

    // 注册一次请求，返回 requestId（调用方持有 m_mutex）
    qint64 registerPending(int msgCode);

    // 供FdbusClientImpl回调（在主线程通过 QueuedConnection 触发）
    friend class FdbusClientImpl;
    void handleReply(int msgCode, const QJsonObject &data);
    void handleError(int msgCode, const QString &error);
    void handleOnline();
    void handleOffline();
};

#endif // FDBUSCLIENTSERVICE_H
