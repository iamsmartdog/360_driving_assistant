#ifndef USERINFO_H
#define USERINFO_H

#include <QString>

// 用户登录信息数据结构
struct UserInfo
{
    QString username;      // 用户名
    QString password;      // 密码（MD5加密后）
    QString nickname;      // 用户昵称
};

#endif // USERINFO_H