#ifndef USERMODEL_H
#define USERMODEL_H

#include <string>

// 用户数据模型（纯数据结构，不包含业务逻辑）
struct UserModel
{
    int id = 0;
    std::string username;
    std::string password_hash;  // 密码哈希（PBKDF2 格式或旧版 MD5）
    std::string nickname;
    std::string created_at;
};

#endif // USERMODEL_H
