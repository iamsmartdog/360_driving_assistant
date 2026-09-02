#include "DrivingAssistantServer.h"
#include "driving_assistant.pb.h"
#include "ServerLogger.h"

DrivingAssistantServer::DrivingAssistantServer(const char *name, CBaseWorker *worker,
                                               MySqlDao &db, UserDao &userDao,
                                               VideoDao &videoDao,
                                               ScreenshotDao &screenshotDao,
                                               AuthService &authService)
    : CBaseServer(name, worker)
    , m_db(db)
    , m_userDao(userDao)
    , m_videoDao(videoDao)
    , m_screenshotDao(screenshotDao)
    , m_authService(authService)
{
}

void DrivingAssistantServer::onOnline(const CFdbOnlineInfo &info)
{
    LOG_INFO("客户端已连接: SID=%d, QOS=%d", info.mSid, info.mQOS);
}

void DrivingAssistantServer::onOffline(const CFdbOnlineInfo &info)
{
    LOG_INFO("客户端已断开: SID=%d, QOS=%d", info.mSid, info.mQOS);
}

void DrivingAssistantServer::onInvoke(CBaseJob::Ptr &msg_ref)
{
    auto msg = castToMessage<CBaseMessage *>(msg_ref);

    LOG_INFO("收到请求: 消息ID=%d, 发送者=%s", msg->code(), msg->senderName().c_str());

    switch (msg->code()) {
    case REQ_LOGIN:
        handleLogin(msg_ref, msg);
        break;
    case REQ_REGISTER:
        handleRegister(msg_ref, msg);
        break;
    case REQ_HEARTBEAT:
        handleHeartbeat(msg_ref, msg);
        break;
    case REQ_GET_VIDEO_LIST:
        handleGetVideoList(msg_ref, msg);
        break;
    case REQ_UPLOAD_VIDEO:
        handleUploadVideo(msg_ref, msg);
        break;
    case REQ_DELETE_VIDEO:
        handleDeleteVideo(msg_ref, msg);
        break;
    case REQ_UPDATE_PLAY_RECORD:
        handleUpdatePlayRecord(msg_ref, msg);
        break;
    case REQ_UPLOAD_SCREENSHOT:
        handleUploadScreenshot(msg_ref, msg);
        break;
    case REQ_GET_SCREENSHOT_LIST:
        handleGetScreenshotList(msg_ref, msg);
        break;
    default:
        msg->status(msg_ref, FDB_ST_NOT_IMPLEMENTED, "未知请求类型");
        break;
    }
}

void DrivingAssistantServer::handleLogin(CBaseJob::Ptr &msg_ref, CBaseMessage *msg)
{
    driving_assistant::LoginRequest request;
    CFdbProtoMsgParser parser(request);
    if (!msg->deserialize(parser)) {
        msg->status(msg_ref, FDB_ST_MSG_DECODE_FAIL, "登录请求解析失败");
        return;
    }

    LOG_INFO("登录请求: username=%s", request.username().c_str());

    auto result = m_authService.login(request.username(), request.password());

    driving_assistant::LoginResponse response;
    response.set_success(result.success);
    response.set_message(result.message);
    if (result.success) {
        response.set_nickname(result.nickname);
        response.set_user_id(result.user_id);
    }

    CFdbProtoMsgBuilder builder(response);
    msg->reply(msg_ref, builder);
}

void DrivingAssistantServer::handleRegister(CBaseJob::Ptr &msg_ref, CBaseMessage *msg)
{
    driving_assistant::RegisterRequest request;
    CFdbProtoMsgParser parser(request);
    if (!msg->deserialize(parser)) {
        msg->status(msg_ref, FDB_ST_MSG_DECODE_FAIL, "注册请求解析失败");
        return;
    }

    LOG_INFO("注册请求: username=%s, nickname=%s",
             request.username().c_str(), request.nickname().c_str());

    auto result = m_authService.registerUser(
        request.username(), request.password(), request.nickname());

    driving_assistant::RegisterResponse response;
    response.set_success(result.success);
    response.set_message(result.message);

    CFdbProtoMsgBuilder builder(response);
    msg->reply(msg_ref, builder);
}

void DrivingAssistantServer::handleHeartbeat(CBaseJob::Ptr &msg_ref, CBaseMessage *msg)
{
    driving_assistant::HeartbeatRequest request;
    CFdbProtoMsgParser parser(request);
    if (!msg->deserialize(parser)) {
        msg->status(msg_ref, FDB_ST_MSG_DECODE_FAIL, "心跳请求解析失败");
        return;
    }

    driving_assistant::HeartbeatResponse response;
    response.set_success(true);
    response.set_timestamp(request.timestamp());
    response.set_server_version("1.0.0");

    CFdbProtoMsgBuilder builder(response);
    msg->reply(msg_ref, builder);
}

void DrivingAssistantServer::handleGetVideoList(CBaseJob::Ptr &msg_ref, CBaseMessage *msg)
{
    driving_assistant::GetVideoListRequest request;
    CFdbProtoMsgParser parser(request);
    if (!msg->deserialize(parser)) {
        msg->status(msg_ref, FDB_ST_MSG_DECODE_FAIL, "视频列表请求解析失败");
        return;
    }

    int page = request.has_page() ? request.page() : 1;
    int pageSize = request.has_page_size() ? request.page_size() : 20;
    std::string username = request.has_username() ? request.username() : "";

    auto records = m_videoDao.getVideoList(username, page, pageSize);

    driving_assistant::GetVideoListResponse response;
    response.set_success(true);

    for (const auto &record : records) {
        auto *pbRecord = response.add_records();
        pbRecord->set_id(record.id);
        pbRecord->set_username(record.username);
        pbRecord->set_video_name(record.video_name);
        pbRecord->set_video_size(record.video_size);
        pbRecord->set_video_path(record.video_path);
        pbRecord->set_record_type(record.record_type);
        pbRecord->set_created_at(record.created_at);
        // 视频详情
        pbRecord->set_duration_sec(record.duration_sec);
        pbRecord->set_resolution(record.resolution);
        pbRecord->set_fps(record.fps);
        pbRecord->set_camera_source(record.camera_source);
        // 播放记录
        pbRecord->set_last_play_sec(record.last_play_sec);
        pbRecord->set_play_count(record.play_count);
    }
    // total_count 返回满足过滤条件的真实总数（而非当前页记录数），供客户端计算分页
    response.set_total_count(m_videoDao.getVideoCount(username));

    CFdbProtoMsgBuilder builder(response);
    msg->reply(msg_ref, builder);
}

void DrivingAssistantServer::handleUploadVideo(CBaseJob::Ptr &msg_ref, CBaseMessage *msg)
{
    driving_assistant::UploadVideoRequest request;
    CFdbProtoMsgParser parser(request);
    if (!msg->deserialize(parser)) {
        msg->status(msg_ref, FDB_ST_MSG_DECODE_FAIL, "上传视频请求解析失败");
        return;
    }

    bool ok = m_videoDao.createVideoRecord(
        request.username(),
        request.video_name(),
        request.video_size(),
        request.has_video_path() ? request.video_path() : "",
        request.has_record_type() ? request.record_type() : "",
        request.has_duration_sec() ? request.duration_sec() : 0,
        request.has_resolution() ? request.resolution() : "",
        request.has_fps() ? request.fps() : 0,
        request.has_camera_source() ? request.camera_source() : ""
    );

    driving_assistant::GeneralResponse response;
    response.set_success(ok);
    response.set_message(ok ? "上传成功" : "上传失败");

    CFdbProtoMsgBuilder builder(response);
    msg->reply(msg_ref, builder);
}

void DrivingAssistantServer::handleDeleteVideo(CBaseJob::Ptr &msg_ref, CBaseMessage *msg)
{
    driving_assistant::DeleteVideoRequest request;
    CFdbProtoMsgParser parser(request);
    if (!msg->deserialize(parser)) {
        msg->status(msg_ref, FDB_ST_MSG_DECODE_FAIL, "删除视频请求解析失败");
        return;
    }

    bool ok = m_videoDao.deleteVideo(request.video_id(), request.username());

    driving_assistant::GeneralResponse response;
    response.set_success(ok);
    response.set_message(ok ? "删除成功" : "删除失败或无权限");

    CFdbProtoMsgBuilder builder(response);
    msg->reply(msg_ref, builder);
}

void DrivingAssistantServer::handleUpdatePlayRecord(CBaseJob::Ptr &msg_ref, CBaseMessage *msg)
{
    driving_assistant::UpdatePlayRecordRequest request;
    CFdbProtoMsgParser parser(request);
    if (!msg->deserialize(parser)) {
        msg->status(msg_ref, FDB_ST_MSG_DECODE_FAIL, "更新播放记录请求解析失败");
        return;
    }

    LOG_INFO("更新播放记录: videoId=%d, username=%s, lastPlaySec=%d",
             request.video_id(), request.username().c_str(), request.last_play_sec());

    bool ok = m_videoDao.updatePlayRecord(
        request.video_id(), request.username(), request.last_play_sec());

    driving_assistant::GeneralResponse response;
    response.set_success(ok);
    response.set_message(ok ? "更新成功" : "更新失败");

    CFdbProtoMsgBuilder builder(response);
    msg->reply(msg_ref, builder);
}

void DrivingAssistantServer::handleUploadScreenshot(CBaseJob::Ptr &msg_ref, CBaseMessage *msg)
{
    driving_assistant::UploadScreenshotRequest request;
    CFdbProtoMsgParser parser(request);
    if (!msg->deserialize(parser)) {
        msg->status(msg_ref, FDB_ST_MSG_DECODE_FAIL, "上传截图请求解析失败");
        return;
    }

    LOG_INFO("上传截图: username=%s, name=%s, size=%dKB, type=%s",
             request.username().c_str(), request.screenshot_name().c_str(),
             request.screenshot_size(),
             request.has_record_type() ? request.record_type().c_str() : "");

    bool ok = m_screenshotDao.createScreenshotRecord(
        request.username(),
        request.screenshot_name(),
        request.screenshot_size(),
        request.has_screenshot_path() ? request.screenshot_path() : "",
        request.has_record_type() ? request.record_type() : "",
        request.has_detection_info() ? request.detection_info() : "",
        request.has_resolution() ? request.resolution() : "",
        request.has_camera_source() ? request.camera_source() : ""
    );

    driving_assistant::GeneralResponse response;
    response.set_success(ok);
    response.set_message(ok ? "截图上传成功" : "截图上传失败");

    CFdbProtoMsgBuilder builder(response);
    msg->reply(msg_ref, builder);
}

void DrivingAssistantServer::handleGetScreenshotList(CBaseJob::Ptr &msg_ref, CBaseMessage *msg)
{
    driving_assistant::GetScreenshotListRequest request;
    CFdbProtoMsgParser parser(request);
    if (!msg->deserialize(parser)) {
        msg->status(msg_ref, FDB_ST_MSG_DECODE_FAIL, "截图列表请求解析失败");
        return;
    }

    int page = request.has_page() ? request.page() : 1;
    int pageSize = request.has_page_size() ? request.page_size() : 20;
    std::string username = request.has_username() ? request.username() : "";
    std::string recordType = request.has_record_type() ? request.record_type() : "";

    auto records = m_screenshotDao.getScreenshotList(username, recordType, page, pageSize);

    driving_assistant::GetScreenshotListResponse response;
    response.set_success(true);

    for (const auto &record : records) {
        auto *pbRecord = response.add_records();
        pbRecord->set_id(record.id);
        pbRecord->set_username(record.username);
        pbRecord->set_screenshot_name(record.screenshot_name);
        pbRecord->set_screenshot_size(record.screenshot_size);
        pbRecord->set_screenshot_path(record.screenshot_path);
        pbRecord->set_record_type(record.record_type);
        pbRecord->set_detection_info(record.detection_info);
        pbRecord->set_resolution(record.resolution);
        pbRecord->set_camera_source(record.camera_source);
        pbRecord->set_created_at(record.created_at);
    }
    // total_count 返回满足过滤条件的真实总数（而非当前页记录数），供客户端计算分页
    response.set_total_count(m_screenshotDao.getScreenshotCount(username, recordType));

    CFdbProtoMsgBuilder builder(response);
    msg->reply(msg_ref, builder);
}
