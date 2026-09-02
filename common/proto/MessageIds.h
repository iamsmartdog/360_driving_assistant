#ifndef DRIVING_ASSISTANT_MESSAGE_IDS_H
#define DRIVING_ASSISTANT_MESSAGE_IDS_H

// ============================================================================
// FDBus 消息ID — 客户端与服务端共享的单一事实来源
//
// 这些 ID 属于 wire protocol 的一部分，客户端与服务端必须严格一致。
// 历史上客户端 (FdbusClientService.cpp) 与服务端 (DrivingAssistantServer.h)
// 各自定义一份枚举，容易出现双向漂移；现统一收敛到本头文件。
//
// 设计说明：
//   使用未限定作用域枚举（unscoped enum）以隐式转换为 int，与 FDBus 的
//   FdbMsgCode_t / msg->code() / invoke(code, ...) 等 int 接口兼容。
//   若改用 enum class，需在每一处 switch/invoke 处加 static_cast，收益不抵成本。
//
// 新增消息时：
//   1. 在此追加 REQ_XXX = N（勿复用已废弃编号）
//   2. 客户端 FdbusClientService 补 send 方法 + onReply 分支
//   3. 服务端 DrivingAssistantServer 补 onInvoke 分支 + handle 方法
// ============================================================================

enum MessageId
{
    REQ_LOGIN = 1,
    REQ_REGISTER = 2,
    REQ_HEARTBEAT = 3,
    REQ_GET_VIDEO_LIST = 4,
    REQ_DELETE_VIDEO = 5,
    REQ_UPLOAD_VIDEO = 6,
    REQ_GET_CONFIG = 7,
    REQ_UPDATE_PLAY_RECORD = 8,
    REQ_UPLOAD_SCREENSHOT = 9,
    REQ_GET_SCREENSHOT_LIST = 10
};

#endif // DRIVING_ASSISTANT_MESSAGE_IDS_H
