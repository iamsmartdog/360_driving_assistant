#ifndef AUTHSERVICE_H
#define AUTHSERVICE_H

#include <QObject>
#include <QString>
#include <QJsonObject>
#include "UserInfo.h"

class FdbusClientService;

/**
 * @brief 认证服务类
 * 负责用户登录注册：MD5加密、通过FDBus发送请求到服务器
 * 使用FdbusClientService单例通信
 */
class AuthService : public QObject
{
    Q_OBJECT

public:
    explicit AuthService(QObject *parent = nullptr);

    // 登录验证（通过FDBus发送请求到服务器）
    void login(const QString &username, const QString &password);

    // 注册用户（通过FDBus发送请求到服务器）
    void registerUser(const QString &username, const QString &password, const QString &nickname);

signals:
    void loginSuccess(const QString &nickname);
    void loginFailed(const QString &errorMessage);
    void registerSuccess();
    void registerFailed(const QString &errorMessage);

private slots:
    void onFdbusResponse(qint64 requestId, const QJsonObject &responseData);
    void onFdbusError(qint64 requestId, const QString &errorMessage);

private:
    enum class PendingAction { None, Login, Register };
    PendingAction m_pendingAction = PendingAction::None;
    qint64 m_pendingRequestId = 0;   // 请求-响应关联令牌
    QString m_pendingNickname;
};

#endif // AUTHSERVICE_H
