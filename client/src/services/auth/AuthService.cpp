#include "AuthService.h"
#include "FdbusClientService.h"
#include <QJsonObject>
#include <QDebug>

AuthService::AuthService(QObject *parent)
    : QObject(parent)
{
    // 连接FDBus客户端单例的信号
    auto &fdbus = FdbusClientService::instance();
    connect(&fdbus, &FdbusClientService::requestSuccess,
            this, &AuthService::onFdbusResponse);
    connect(&fdbus, &FdbusClientService::requestFailed,
            this, &AuthService::onFdbusError);
}

void AuthService::login(const QString &username, const QString &password)
{
    qDebug() << "AuthService: 发起登录请求 -" << username;

    // 明文密码传输到服务端，由服务端进行 PBKDF2 加盐哈希验证
    m_pendingAction = PendingAction::Login;
    m_pendingRequestId = FdbusClientService::instance().sendLoginRequest(username, password);
    if (m_pendingRequestId == 0) {
        emit loginFailed("网络未连接");
        m_pendingAction = PendingAction::None;
    }
}

void AuthService::registerUser(const QString &username, const QString &password, const QString &nickname)
{
    qDebug() << "AuthService: 发起注册请求 -" << username << nickname;

    m_pendingAction = PendingAction::Register;
    m_pendingNickname = nickname;
    m_pendingRequestId = FdbusClientService::instance().sendRegisterRequest(username, password, nickname);
    if (m_pendingRequestId == 0) {
        emit registerFailed("网络未连接");
        m_pendingAction = PendingAction::None;
    }
}

void AuthService::onFdbusResponse(qint64 requestId, const QJsonObject &responseData)
{
    // 仅处理自己发起的请求，避免被其他 ViewModel 的响应串扰
    if (requestId != m_pendingRequestId) return;

    qDebug() << "AuthService: 收到FDBus响应" << responseData;

    switch (m_pendingAction) {
    case PendingAction::Login:
        if (responseData.value("success").toBool()) {
            QString nickname = responseData.value("nickname").toString();
            emit loginSuccess(nickname);
        } else {
            QString error = responseData.value("message").toString("登录失败");
            emit loginFailed(error);
        }
        break;

    case PendingAction::Register:
        if (responseData.value("success").toBool()) {
            emit registerSuccess();
        } else {
            QString error = responseData.value("message").toString("注册失败");
            emit registerFailed(error);
        }
        break;

    default:
        break;
    }

    m_pendingAction = PendingAction::None;
    m_pendingRequestId = 0;
}

void AuthService::onFdbusError(qint64 requestId, const QString &errorMessage)
{
    if (requestId != m_pendingRequestId) return;

    qWarning() << "AuthService: FDBus错误 -" << errorMessage;

    switch (m_pendingAction) {
    case PendingAction::Login:
        emit loginFailed("网络错误：" + errorMessage);
        break;
    case PendingAction::Register:
        emit registerFailed("网络错误：" + errorMessage);
        break;
    default:
        break;
    }

    m_pendingAction = PendingAction::None;
    m_pendingRequestId = 0;
}
