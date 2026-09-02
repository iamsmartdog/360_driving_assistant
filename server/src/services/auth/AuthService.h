#ifndef AUTHSERVICE_SERVER_H
#define AUTHSERVICE_SERVER_H

#include "UserDao.h"
#include "VideoDao.h"
#include <string>

/**
 * @brief 服务端认证服务
 * 处理用户登录、注册的业务逻辑
 */
class AuthService
{
public:
    AuthService(UserDao &userDao, VideoDao &videoDao);

    // 登录验证，返回{success, message, nickname, userId}
    struct LoginResult {
        bool success = false;
        std::string message;
        std::string nickname;
        int user_id = 0;
    };
    LoginResult login(const std::string &username, const std::string &password);

    // 注册，返回{success, message}
    struct RegisterResult {
        bool success = false;
        std::string message;
    };
    RegisterResult registerUser(const std::string &username, const std::string &password,
                                 const std::string &nickname);

private:
    UserDao &m_userDao;
    VideoDao &m_videoDao;
};

#endif // AUTHSERVICE_SERVER_H
