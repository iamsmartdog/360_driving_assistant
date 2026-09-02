#include "LoginViewModel.h"
#include "services/auth/SessionContext.h"
#include <QDebug>
#include <QRandomGenerator>
#include <QRegularExpression>

LoginViewModel::LoginViewModel(QObject *parent)
    : QObject(parent)
{
    // 转发AuthService的信号给QML
    connect(&m_authService, &AuthService::loginSuccess,
            this, &LoginViewModel::onLoginSuccess);
    connect(&m_authService, &AuthService::loginFailed,
            this, &LoginViewModel::loginFailed);
    connect(&m_authService, &AuthService::registerSuccess,
            this, &LoginViewModel::onRegisterSuccess);
    connect(&m_authService, &AuthService::registerFailed,
            this, &LoginViewModel::registerFailed);
}

QString LoginViewModel::captchaCode() const { return m_captchaCode; }
QString LoginViewModel::currentUserNickname() const { return m_currentUserNickname; }
QString LoginViewModel::currentAccount() const { return m_currentAccount; }

void LoginViewModel::generateCaptcha()
{
    // 生成4位验证码：字母+数字
    const QString chars = "ABCDEFGHJKLMNPQRSTUVWXYZabcdefghjkmnpqrstuvwxyz23456789";
    QString code;
    for (int i = 0; i < 4; ++i) {
        int index = QRandomGenerator::global()->bounded(chars.length());
        code.append(chars[index]);
    }
    m_captchaCode = code;
    qDebug() << "生成验证码:" << code;
    emit captchaCodeChanged();
}

bool LoginViewModel::validateCaptcha(const QString &input)
{
    // 不区分大小写
    return input.toUpper() == m_captchaCode.toUpper();
}

QString LoginViewModel::validateAccount(const QString &account)
{
    // 账号：英文大小写、数字、下划线，11位
    if (account.isEmpty()) return "账号不能为空";
    if (account.length() != 11)
        return "账号长度需要11位";

    QRegularExpression re("^[a-zA-Z0-9_]{11}$");
    if (!re.match(account).hasMatch())
        return "账号只能包含英文字母、数字、下划线";

    return "";
}

QString LoginViewModel::validateUsername(const QString &username)
{
    // 用户名：中英文大小写、数字，6~8位（注册时用）
    if (username.isEmpty()) return "用户名不能为空";
    if (username.length() < 6 || username.length() > 8)
        return "用户名长度需要6~8位";

    QRegularExpression re("^[a-zA-Z0-9\\u4e00-\\u9fa5]{6,8}$");
    if (!re.match(username).hasMatch())
        return "用户名只能包含中英文字母、数字";

    return "";
}

QString LoginViewModel::validatePassword(const QString &password)
{
    // 密码：英文大小写、数字、下划线，6~10位
    if (password.isEmpty()) return "密码不能为空";
    if (password.length() < 6 || password.length() > 10)
        return "密码长度需要6~10位";

    // 拒绝特殊字符@#￥
    QRegularExpression invalidRe("[@#￥]");
    if (invalidRe.match(password).hasMatch())
        return "密码不能包含@#￥等特殊字符";

    QRegularExpression re("^[a-zA-Z0-9_]{6,10}$");
    if (!re.match(password).hasMatch())
        return "密码只能包含英文字母、数字、下划线";

    return "";
}

void LoginViewModel::login(const QString &account, const QString &password, const QString &captcha)
{
    // 1. 验证验证码
    if (!validateCaptcha(captcha)) {
        emit validationError("验证码错误，请重新输入");
        generateCaptcha();
        return;
    }

    // 2. 验证账号
    QString accountError = validateAccount(account);
    if (!accountError.isEmpty()) {
        emit validationError(accountError);
        return;
    }

    // 3. 验证密码
    QString passwordError = validatePassword(password);
    if (!passwordError.isEmpty()) {
        emit validationError(passwordError);
        return;
    }

    // 4. 保存当前账号
    m_currentAccount = account;
    emit currentAccountChanged();

    // 5. 调用AuthService登录
    m_authService.login(account, password);
}

void LoginViewModel::registerUser(const QString &username, const QString &password, const QString &nickname)
{
    // 1. 验证昵称
    if (username.isEmpty()) {
        emit validationError("昵称不能为空");
        return;
    }

    // 2. 验证密码
    QString passwordError = validatePassword(password);
    if (!passwordError.isEmpty()) {
        emit validationError(passwordError);
        return;
    }

    // 3. 自动生成11位随机数字账号
    QString account;
    for (int i = 0; i < 11; ++i) {
        account.append(QChar('0' + QRandomGenerator::global()->bounded(10)));
    }
    // 确保首位不为0
    account[0] = QChar('1' + QRandomGenerator::global()->bounded(9));
    m_currentAccount = account;
    emit currentAccountChanged();

    qDebug() << "注册：自动生成账号:" << account;

    // 4. 调用AuthService注册（用生成的账号替代username）
    m_authService.registerUser(account, password, nickname);
}

void LoginViewModel::onLoginSuccess(const QString &nickname)
{
    m_currentUserNickname = nickname;
    emit currentUserNicknameChanged();
    // 写入全局会话上下文，供其他 ViewModel 共享登录态
    SessionContext::instance().setLoginInfo(m_currentAccount, nickname);
    emit loginSuccess(nickname);
}

void LoginViewModel::onRegisterSuccess()
{
    emit registerSuccess();
}
