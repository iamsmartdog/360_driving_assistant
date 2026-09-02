#include "AuthService.h"
#include "utils/PasswordHasher.h"
#include "ServerLogger.h"

AuthService::AuthService(UserDao &userDao, VideoDao &videoDao)
    : m_userDao(userDao)
    , m_videoDao(videoDao)
{
}

AuthService::LoginResult AuthService::login(const std::string &username, const std::string &password)
{
    LoginResult result;

    // 查找用户
    auto user = m_userDao.findByUsername(username);
    if (!user) {
        result.success = false;
        result.message = "账号不存在";
        return result;
    }

    // 验证密码（自动兼容旧版 MD5 哈希）
    if (!PasswordHasher::verify(password, user->password_hash)) {
        result.success = false;
        result.message = "密码错误";
        return result;
    }

    // 旧版 MD5 哈希验证通过后，自动升级为 PBKDF2
    if (PasswordHasher::needsUpgrade(user->password_hash)) {
        std::string newHash = PasswordHasher::hash(password);
        if (!newHash.empty()) {
            m_userDao.updatePasswordHash(username, newHash);
            LOG_INFO("用户 %s 密码哈希已从 MD5 升级到 PBKDF2", username.c_str());
        }
    }

    // 登录成功
    result.success = true;
    result.message = "登录成功";
    result.nickname = user->nickname;
    result.user_id = user->id;

    LOG_INFO("用户登录成功: %s (昵称: %s)", username.c_str(), user->nickname.c_str());
    return result;
}

AuthService::RegisterResult AuthService::registerUser(const std::string &username,
                                                       const std::string &password,
                                                       const std::string &nickname)
{
    RegisterResult result;

    // 检查用户名是否已存在
    if (m_userDao.existsByUsername(username)) {
        result.success = false;
        result.message = "账号已存在";
        return result;
    }

    // 加盐哈希后存储
    std::string passwordHash = PasswordHasher::hash(password);
    if (passwordHash.empty()) {
        result.success = false;
        result.message = "注册失败，请稍后重试";
        return result;
    }

    if (!m_userDao.createUser(username, passwordHash, nickname)) {
        result.success = false;
        result.message = "注册失败，请稍后重试";
        return result;
    }

    result.success = true;
    result.message = "注册成功";

    LOG_INFO("用户注册成功: %s (昵称: %s)", username.c_str(), nickname.c_str());
    return result;
}
