#ifndef SESSIONCONTEXT_H
#define SESSIONCONTEXT_H

#include <QObject>
#include <QString>

/**
 * @brief 全局会话上下文（单例）
 *
 * 集中持有当前登录用户信息，供所有 ViewModel/Service 共享。
 * 取代原先每个 ViewModel 各存一份 m_currentUsername、且登录成功后不同步的问题
 * （原先各 ViewModel 一直用默认 "test_user" 或空串发请求）。
 *
 * 登录成功后由 LoginViewModel 调用 setLoginInfo 写入；
 * 其余 ViewModel 在发请求时直接读取 username()。
 */
class SessionContext : public QObject
{
    Q_OBJECT
    Q_PROPERTY(QString username READ username NOTIFY usernameChanged)
    Q_PROPERTY(QString nickname READ nickname NOTIFY nicknameChanged)
    Q_PROPERTY(bool isLoggedIn READ isLoggedIn NOTIFY usernameChanged)

public:
    static SessionContext& instance();

    QString username() const;
    QString nickname() const;
    bool isLoggedIn() const;

    // 登录成功后调用
    void setLoginInfo(const QString &username, const QString &nickname);

    // 登出
    void clear();

signals:
    void usernameChanged();
    void nicknameChanged();
    void loggedIn(const QString &username, const QString &nickname);
    void loggedOut();

private:
    SessionContext(QObject *parent = nullptr);
    QString m_username;
    QString m_nickname;
};

#endif // SESSIONCONTEXT_H
