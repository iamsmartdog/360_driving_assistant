#ifndef LOGINVIEWMODEL_H
#define LOGINVIEWMODEL_H

#include <QObject>
#include <QString>
#include "AuthService.h"

/**
 * @brief 登录/注册视图模型
 * 负责验证码生成、输入验证、调用AuthService
 * 验证规则：
 *   账号：英文大小写、数字、下划线，6~8位
 *   用户名：中英文大小写、数字，6~8位（注册时用）
 *   密码：英文大小写、数字、下划线，6~10位
 *   验证码：字母+数字，不区分大小写
 */
class LoginViewModel : public QObject
{
    Q_OBJECT

    Q_PROPERTY(QString captchaCode READ captchaCode NOTIFY captchaCodeChanged)
    Q_PROPERTY(QString currentUserNickname READ currentUserNickname NOTIFY currentUserNicknameChanged)
    Q_PROPERTY(QString currentAccount READ currentAccount NOTIFY currentAccountChanged)

public:
    explicit LoginViewModel(QObject *parent = nullptr);

    QString captchaCode() const;
    QString currentUserNickname() const;
    QString currentAccount() const;

    // 登录相关
    Q_INVOKABLE void generateCaptcha();
    Q_INVOKABLE bool validateCaptcha(const QString &input);
    Q_INVOKABLE QString validateAccount(const QString &account);
    Q_INVOKABLE QString validateUsername(const QString &username);
    Q_INVOKABLE QString validatePassword(const QString &password);
    Q_INVOKABLE void login(const QString &account, const QString &password, const QString &captcha);

    // 注册相关
    Q_INVOKABLE void registerUser(const QString &username, const QString &password, const QString &nickname);

signals:
    void captchaCodeChanged();
    void currentUserNicknameChanged();
    void currentAccountChanged();

    void loginSuccess(const QString &nickname);
    void loginFailed(const QString &errorMessage);
    void registerSuccess();
    void registerFailed(const QString &errorMessage);
    void validationError(const QString &errorMessage);

private slots:
    void onLoginSuccess(const QString &nickname);
    void onRegisterSuccess();

private:
    AuthService m_authService;
    QString m_captchaCode;
    QString m_currentUserNickname;
    QString m_currentAccount;
};

#endif // LOGINVIEWMODEL_H
