#include "FdbusClientService.h"
#include "services/config/AppConfig.h"
#include <QDebug>
#include <QMetaObject>
#include <QCoreApplication>
#include <QDateTime>
#include <QJsonArray>
#include <QJsonObject>

// FDBus头文件
#include <fdbus/fdbus.h>
#include <fdbus/CFdbProtoMsgBuilder.h>

// Protobuf生成头文件
#include "driving_assistant.pb.h"

// 客户端/服务端共享的消息ID定义（单一事实来源）
#include "proto/MessageIds.h"

using namespace ipc::fdbus;

// ==================== FDBus客户端实现 ====================
class FdbusClientService::FdbusClientImpl : public CBaseClient
{
public:
    FdbusClientImpl(const char *name, CBaseWorker *worker, FdbusClientService *owner)
        : CBaseClient(name, worker)
        , m_owner(owner)
    {
    }

protected:
    void onOnline(const CFdbOnlineInfo &info) override
    {
        Q_UNUSED(info)
        qDebug() << "FDBus: 服务器已连接";
        QMetaObject::invokeMethod(m_owner, [this]() {
            m_owner->handleOnline();
        }, Qt::QueuedConnection);
    }

    void onOffline(const CFdbOnlineInfo &info) override
    {
        Q_UNUSED(info)
        qDebug() << "FDBus: 服务器断开连接";
        QMetaObject::invokeMethod(m_owner, [this]() {
            m_owner->handleOffline();
        }, Qt::QueuedConnection);
    }

    void onReply(CBaseJob::Ptr &msg_ref) override
    {
        auto msg = castToMessage<CBaseMessage *>(msg_ref);

        if (msg->isStatus()) {
            int32_t error_code = 0;
            std::string reason;
            if (!msg->decodeStatus(error_code, reason)) {
                reason = "未知错误";
            }
            QString error = QString("错误%1: %2").arg(error_code).arg(QString::fromStdString(reason));
            int code = msg->code();

            QMetaObject::invokeMethod(m_owner, [this, code, error]() {
                m_owner->handleError(code, error);
            }, Qt::QueuedConnection);
            return;
        }

        int msgCode = msg->code();
        QJsonObject responseData;

        switch (msgCode) {
        case REQ_LOGIN: {
            driving_assistant::LoginResponse response;
            CFdbProtoMsgParser parser(response);
            if (msg->deserialize(parser)) {
                responseData["success"] = response.success();
                if (response.has_nickname())
                    responseData["nickname"] = QString::fromStdString(response.nickname());
                if (response.has_user_id())
                    responseData["user_id"] = response.user_id();
                if (response.has_message())
                    responseData["message"] = QString::fromStdString(response.message());
            }
            break;
        }
        case REQ_REGISTER: {
            driving_assistant::RegisterResponse response;
            CFdbProtoMsgParser parser(response);
            if (msg->deserialize(parser)) {
                responseData["success"] = response.success();
                if (response.has_message())
                    responseData["message"] = QString::fromStdString(response.message());
            }
            break;
        }
        case REQ_HEARTBEAT: {
            driving_assistant::HeartbeatResponse response;
            CFdbProtoMsgParser parser(response);
            if (msg->deserialize(parser)) {
                responseData["success"] = response.success();
            }
            break;
        }
        case REQ_GET_VIDEO_LIST: {
            driving_assistant::GetVideoListResponse response;
            CFdbProtoMsgParser parser(response);
            if (msg->deserialize(parser)) {
                responseData["success"] = response.success();
                responseData["total_count"] = response.total_count();
                QJsonArray records;
                for (int i = 0; i < response.records_size(); i++) {
                    const auto &r = response.records(i);
                    QJsonObject rec;
                    rec["id"] = r.id();
                    rec["username"] = QString::fromStdString(r.username());
                    rec["video_name"] = QString::fromStdString(r.video_name());
                    rec["video_size"] = r.video_size();
                    rec["video_path"] = QString::fromStdString(r.video_path());
                    rec["record_type"] = QString::fromStdString(r.record_type());
                    rec["created_at"] = QString::fromStdString(r.created_at());
                    if (r.has_duration_sec()) rec["duration_sec"] = r.duration_sec();
                    if (r.has_resolution()) rec["resolution"] = QString::fromStdString(r.resolution());
                    if (r.has_fps()) rec["fps"] = r.fps();
                    if (r.has_camera_source()) rec["camera_source"] = QString::fromStdString(r.camera_source());
                    if (r.has_last_play_sec()) rec["last_play_sec"] = r.last_play_sec();
                    if (r.has_play_count()) rec["play_count"] = r.play_count();
                    records.append(rec);
                }
                responseData["records"] = records;
            }
            break;
        }
        case REQ_UPLOAD_VIDEO:
        case REQ_DELETE_VIDEO:
        case REQ_UPDATE_PLAY_RECORD:
        case REQ_UPLOAD_SCREENSHOT: {
            driving_assistant::GeneralResponse response;
            CFdbProtoMsgParser parser(response);
            if (msg->deserialize(parser)) {
                responseData["success"] = response.success();
                if (response.has_message())
                    responseData["message"] = QString::fromStdString(response.message());
            }
            break;
        }
        case REQ_GET_SCREENSHOT_LIST: {
            driving_assistant::GetScreenshotListResponse response;
            CFdbProtoMsgParser parser(response);
            if (msg->deserialize(parser)) {
                responseData["success"] = response.success();
                responseData["total_count"] = response.total_count();
                QJsonArray records;
                for (int i = 0; i < response.records_size(); i++) {
                    const auto &r = response.records(i);
                    QJsonObject rec;
                    rec["id"] = r.id();
                    rec["username"] = QString::fromStdString(r.username());
                    rec["screenshot_name"] = QString::fromStdString(r.screenshot_name());
                    rec["screenshot_size"] = r.screenshot_size();
                    rec["screenshot_path"] = QString::fromStdString(r.screenshot_path());
                    rec["record_type"] = QString::fromStdString(r.record_type());
                    rec["detection_info"] = QString::fromStdString(r.detection_info());
                    rec["resolution"] = QString::fromStdString(r.resolution());
                    rec["camera_source"] = QString::fromStdString(r.camera_source());
                    rec["created_at"] = QString::fromStdString(r.created_at());
                    records.append(rec);
                }
                responseData["records"] = records;
            }
            break;
        }
        default:
            break;
        }

        QMetaObject::invokeMethod(m_owner, [this, msgCode, responseData]() {
            m_owner->handleReply(msgCode, responseData);
        }, Qt::QueuedConnection);
    }

    void onStatus(CBaseJob::Ptr &msg_ref, int32_t error_code, const char *description) override
    {
        Q_UNUSED(msg_ref)
        qDebug() << "FDBus: 状态变化 - 错误码:" << error_code << "描述:" << description;
    }

private:
    FdbusClientService *m_owner;
};

// ==================== FdbusClientService实现 ====================

FdbusClientService& FdbusClientService::instance()
{
    static FdbusClientService inst;
    return inst;
}

FdbusClientService::FdbusClientService(QObject *parent)
    : QObject(parent)
{
    // 启动FDBus上下文（只需启动一次）
    FDB_CONTEXT->start();
}

FdbusClientService::~FdbusClientService()
{
    if (m_impl) {
        m_impl->disconnect();
        delete m_impl;
        m_impl = nullptr;
    }
}

qint64 FdbusClientService::registerPending(int msgCode)
{
    // 调用方持有 m_mutex
    qint64 requestId = m_nextRequestId++;
    m_pendingByCode[msgCode].append(requestId);
    return requestId;
}

bool FdbusClientService::connectByName(const QString &serverName)
{
    QString name = serverName.isEmpty() ? AppConfig::instance().fdbusServerName() : serverName;

    QMutexLocker locker(&m_mutex);

    // 先断开旧连接
    if (m_impl) {
        m_impl->disconnect();
        delete m_impl;
        m_impl = nullptr;
        m_isConnected = false;
    }

    // 创建FDBus worker线程
    static CBaseWorker fdbus_worker;
    static bool worker_started = false;
    if (!worker_started) {
        fdbus_worker.start();
        worker_started = true;
    }

    // 创建FDBus客户端
    QString clientName = name + "_client";
    m_impl = new FdbusClientImpl(clientName.toStdString().c_str(), &fdbus_worker, this);
    m_impl->enableReconnect(true);
    m_impl->enableTimeStamp(true);

    // 通过name_server连接：svc://server_name
    QString url = QString("svc://%1").arg(name);
    m_impl->connect(url.toStdString().c_str());

    qDebug() << "FDBus: 通过name_server连接" << url;
    return true;
}

void FdbusClientService::disconnectFromServer()
{
    QMutexLocker locker(&m_mutex);
    if (m_impl) {
        m_impl->disconnect();
        delete m_impl;
        m_impl = nullptr;
    }
    m_isConnected = false;
}

bool FdbusClientService::isConnected() const
{
    return m_isConnected;
}

void FdbusClientService::handleReply(int msgCode, const QJsonObject &data)
{
    qint64 requestId = 0;
    {
        QMutexLocker locker(&m_mutex);
        auto it = m_pendingByCode.find(msgCode);
        if (it != m_pendingByCode.end() && !it->isEmpty()) {
            requestId = it->takeFirst();  // FIFO 队首
            if (it->isEmpty()) m_pendingByCode.erase(it);
        }
    }

    qDebug() << "FDBus: 收到回复 - 消息ID:" << msgCode << "requestId:" << requestId << "数据:" << data;
    if (requestId > 0) {
        emit requestSuccess(requestId, data);
    }
}

void FdbusClientService::handleError(int msgCode, const QString &error)
{
    qint64 requestId = 0;
    {
        QMutexLocker locker(&m_mutex);
        auto it = m_pendingByCode.find(msgCode);
        if (it != m_pendingByCode.end() && !it->isEmpty()) {
            requestId = it->takeFirst();
            if (it->isEmpty()) m_pendingByCode.erase(it);
        }
    }

    qWarning() << "FDBus: 请求失败 - 消息ID:" << msgCode << "requestId:" << requestId << "错误:" << error;
    if (requestId > 0) {
        emit requestFailed(requestId, error);
    }
}

void FdbusClientService::handleOnline()
{
    m_isConnected = true;
    emit serverOnline();
}

void FdbusClientService::handleOffline()
{
    m_isConnected = false;
    emit serverOffline();
}

// ============================================================================
// 各请求：注册 requestId → 发送 → 返回 requestId
// 未连接时返回 0（不进队列、不 emit），由调用方本地处理失败
// ============================================================================

qint64 FdbusClientService::sendLoginRequest(const QString &username, const QString &password)
{
    QMutexLocker locker(&m_mutex);
    if (!m_impl) return 0;

    qint64 requestId = registerPending(REQ_LOGIN);

    driving_assistant::LoginRequest request;
    request.set_username(username.toStdString());
    request.set_password(password.toStdString());

    CFdbProtoMsgBuilder builder(request);
    m_impl->invoke(REQ_LOGIN, builder);

    qDebug() << "FDBus: 发送登录请求 -" << username << "requestId:" << requestId;
    return requestId;
}

qint64 FdbusClientService::sendRegisterRequest(const QString &username, const QString &password, const QString &nickname)
{
    QMutexLocker locker(&m_mutex);
    if (!m_impl) return 0;

    qint64 requestId = registerPending(REQ_REGISTER);

    driving_assistant::RegisterRequest request;
    request.set_username(username.toStdString());
    request.set_password(password.toStdString());
    request.set_nickname(nickname.toStdString());

    CFdbProtoMsgBuilder builder(request);
    m_impl->invoke(REQ_REGISTER, builder);

    qDebug() << "FDBus: 发送注册请求 -" << username << nickname << "requestId:" << requestId;
    return requestId;
}

qint64 FdbusClientService::sendHeartbeat()
{
    QMutexLocker locker(&m_mutex);
    if (!m_impl) return 0;

    qint64 requestId = registerPending(REQ_HEARTBEAT);

    driving_assistant::HeartbeatRequest request;
    request.set_client_id("qt_client");
    request.set_timestamp(QDateTime::currentMSecsSinceEpoch());

    CFdbProtoMsgBuilder builder(request);
    m_impl->invoke(REQ_HEARTBEAT, builder);

    return requestId;
}

qint64 FdbusClientService::sendGetVideoListRequest(const QString &username, int page, int pageSize)
{
    QMutexLocker locker(&m_mutex);
    if (!m_impl) return 0;

    qint64 requestId = registerPending(REQ_GET_VIDEO_LIST);

    driving_assistant::GetVideoListRequest request;
    if (!username.isEmpty())
        request.set_username(username.toStdString());
    request.set_page(page);
    request.set_page_size(pageSize);

    CFdbProtoMsgBuilder builder(request);
    m_impl->invoke(REQ_GET_VIDEO_LIST, builder);

    qDebug() << "FDBus: 发送获取视频列表请求 - username:" << username << "requestId:" << requestId;
    return requestId;
}

qint64 FdbusClientService::sendUploadVideoRequest(const QString &username, const QString &videoName,
                                                   int videoSize, const QString &videoPath,
                                                   const QString &recordType,
                                                   int durationSec, const QString &resolution,
                                                   int fps, const QString &cameraSource)
{
    QMutexLocker locker(&m_mutex);
    if (!m_impl) return 0;

    qint64 requestId = registerPending(REQ_UPLOAD_VIDEO);

    driving_assistant::UploadVideoRequest request;
    request.set_username(username.toStdString());
    request.set_video_name(videoName.toStdString());
    request.set_video_size(videoSize);
    if (!videoPath.isEmpty()) request.set_video_path(videoPath.toStdString());
    if (!recordType.isEmpty()) request.set_record_type(recordType.toStdString());
    if (durationSec > 0) request.set_duration_sec(durationSec);
    if (!resolution.isEmpty()) request.set_resolution(resolution.toStdString());
    if (fps > 0) request.set_fps(fps);
    if (!cameraSource.isEmpty()) request.set_camera_source(cameraSource.toStdString());

    CFdbProtoMsgBuilder builder(request);
    m_impl->invoke(REQ_UPLOAD_VIDEO, builder);

    qDebug() << "FDBus: 发送上传视频记录请求 -" << videoName << "requestId:" << requestId;
    return requestId;
}

qint64 FdbusClientService::sendUpdatePlayRecordRequest(int videoId, const QString &username, int lastPlaySec)
{
    QMutexLocker locker(&m_mutex);
    if (!m_impl) return 0;

    qint64 requestId = registerPending(REQ_UPDATE_PLAY_RECORD);

    driving_assistant::UpdatePlayRecordRequest request;
    request.set_video_id(videoId);
    request.set_username(username.toStdString());
    request.set_last_play_sec(lastPlaySec);

    CFdbProtoMsgBuilder builder(request);
    m_impl->invoke(REQ_UPDATE_PLAY_RECORD, builder);

    qDebug() << "FDBus: 发送更新播放记录请求 - videoId:" << videoId << "requestId:" << requestId;
    return requestId;
}

qint64 FdbusClientService::sendDeleteVideoRequest(int videoId, const QString &username)
{
    QMutexLocker locker(&m_mutex);
    if (!m_impl) return 0;

    qint64 requestId = registerPending(REQ_DELETE_VIDEO);

    driving_assistant::DeleteVideoRequest request;
    request.set_video_id(videoId);
    request.set_username(username.toStdString());

    CFdbProtoMsgBuilder builder(request);
    m_impl->invoke(REQ_DELETE_VIDEO, builder);

    qDebug() << "FDBus: 发送删除视频请求 - videoId:" << videoId << "requestId:" << requestId;
    return requestId;
}

qint64 FdbusClientService::sendUploadScreenshotRequest(const QString &username, const QString &screenshotName,
                                                       int screenshotSizeKB, const QString &screenshotPath,
                                                       const QString &recordType,
                                                       const QString &detectionInfo,
                                                       const QString &resolution,
                                                       const QString &cameraSource)
{
    QMutexLocker locker(&m_mutex);
    if (!m_impl) return 0;

    qint64 requestId = registerPending(REQ_UPLOAD_SCREENSHOT);

    driving_assistant::UploadScreenshotRequest request;
    request.set_username(username.toStdString());
    request.set_screenshot_name(screenshotName.toStdString());
    request.set_screenshot_size(screenshotSizeKB);
    if (!screenshotPath.isEmpty()) request.set_screenshot_path(screenshotPath.toStdString());
    if (!recordType.isEmpty()) request.set_record_type(recordType.toStdString());
    if (!detectionInfo.isEmpty()) request.set_detection_info(detectionInfo.toStdString());
    if (!resolution.isEmpty()) request.set_resolution(resolution.toStdString());
    if (!cameraSource.isEmpty()) request.set_camera_source(cameraSource.toStdString());

    CFdbProtoMsgBuilder builder(request);
    m_impl->invoke(REQ_UPLOAD_SCREENSHOT, builder);

    qDebug() << "FDBus: 发送上传截图请求 -" << screenshotName << "requestId:" << requestId;
    return requestId;
}

qint64 FdbusClientService::sendGetScreenshotListRequest(const QString &username,
                                                        const QString &recordType,
                                                        int page, int pageSize)
{
    QMutexLocker locker(&m_mutex);
    if (!m_impl) return 0;

    qint64 requestId = registerPending(REQ_GET_SCREENSHOT_LIST);

    driving_assistant::GetScreenshotListRequest request;
    if (!username.isEmpty()) request.set_username(username.toStdString());
    if (!recordType.isEmpty()) request.set_record_type(recordType.toStdString());
    request.set_page(page);
    request.set_page_size(pageSize);

    CFdbProtoMsgBuilder builder(request);
    m_impl->invoke(REQ_GET_SCREENSHOT_LIST, builder);

    qDebug() << "FDBus: 发送获取截图列表请求 requestId:" << requestId;
    return requestId;
}
