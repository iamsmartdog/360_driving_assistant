#ifndef USERDAO_H
#define USERDAO_H

#include "MySqlDao.h"
#include "UserModel.h"
#include <string>
#include <optional>

/**
 * @brief 用户数据访问对象
 * 封装用户相关的数据库操作
 */
class UserDao
{
public:
    explicit UserDao(MySqlDao &db);

    // 创建用户（注册）。passwordHash 应为 PasswordHasher::hash() 的输出
    bool createUser(const std::string &username, const std::string &passwordHash,
                    const std::string &nickname);

    // 根据用户名查找用户（登录验证）
    std::optional<UserModel> findByUsername(const std::string &username);

    // 检查用户名是否已存在
    bool existsByUsername(const std::string &username);

    // 更新密码哈希（从旧版 MD5 升级到 PBKDF2 时调用）
    bool updatePasswordHash(const std::string &username, const std::string &passwordHash);

private:
    MySqlDao &m_db;
};

#endif // USERDAO_H
