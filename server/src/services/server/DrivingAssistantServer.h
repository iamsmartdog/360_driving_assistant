#ifndef DRIVINGASSISTANTSERVER_H
#define DRIVINGASSISTANTSERVER_H

#include <fdbus/fdbus.h>
#include <fdbus/CFdbProtoMsgBuilder.h>
#include "MySqlDao.h"
#include "UserDao.h"
#include "VideoDao.h"
#include "ScreenshotDao.h"
#include "AuthService.h"

// 客户端/服务端共享的消息ID定义（单一事实来源）
#include "proto/MessageIds.h"

using namespace ipc::fdbus;

/**
 * @brief 行车辅助系统FDBus服务器
 * 继承CBaseServer，处理客户端请求
 * 通过Protobuf序列化/反序列化消息
 */
class DrivingAssistantServer : public CBaseServer
{
public:
    DrivingAssistantServer(const char *name, CBaseWorker *worker,
                           MySqlDao &db, UserDao &userDao, VideoDao &videoDao,
                           ScreenshotDao &screenshotDao, AuthService &authService);

protected:
    // 客户端连接回调
    void onOnline(const CFdbOnlineInfo &info) override;

    // 客户端断开回调
    void onOffline(const CFdbOnlineInfo &info) override;

    // 接收客户端请求回调
    void onInvoke(CBaseJob::Ptr &msg_ref) override;

private:
    MySqlDao &m_db;
    UserDao &m_userDao;
    VideoDao &m_videoDao;
    ScreenshotDao &m_screenshotDao;
    AuthService &m_authService;

    // 处理登录请求
    void handleLogin(CBaseJob::Ptr &msg_ref, CBaseMessage *msg);

    // 处理注册请求
    void handleRegister(CBaseJob::Ptr &msg_ref, CBaseMessage *msg);

    // 处理心跳请求
    void handleHeartbeat(CBaseJob::Ptr &msg_ref, CBaseMessage *msg);

    // 处理获取视频列表请求
    void handleGetVideoList(CBaseJob::Ptr &msg_ref, CBaseMessage *msg);

    // 处理上传视频请求
    void handleUploadVideo(CBaseJob::Ptr &msg_ref, CBaseMessage *msg);

    // 处理删除视频请求
    void handleDeleteVideo(CBaseJob::Ptr &msg_ref, CBaseMessage *msg);

    // 处理更新播放记录请求
    void handleUpdatePlayRecord(CBaseJob::Ptr &msg_ref, CBaseMessage *msg);

    // 处理上传截图请求
    void handleUploadScreenshot(CBaseJob::Ptr &msg_ref, CBaseMessage *msg);

    // 处理获取截图列表请求
    void handleGetScreenshotList(CBaseJob::Ptr &msg_ref, CBaseMessage *msg);
};

#endif // DRIVINGASSISTANTSERVER_H
