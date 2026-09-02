#include "UserDao.h"
#include "ServerLogger.h"

UserDao::UserDao(MySqlDao &db)
    : m_db(db)
{
}

bool UserDao::createUser(const std::string &username, const std::string &passwordHash,
                          const std::string &nickname)
{
    if (!m_db.isConnected()) return false;

    // 字段统一转义后拼接，防 SQL 注入
    // 注：DB 列名仍为 password_md5（历史遗留），实际存储 PBKDF2 哈希
    std::string sql = "INSERT INTO users (username, password_md5, nickname) VALUES ('"
        + m_db.escapeString(username) + "', '"
        + m_db.escapeString(passwordHash) + "', '"
        + m_db.escapeString(nickname) + "')";

    if (!m_db.executeUpdate(sql)) {
        LOG_ERROR("创建用户失败: %s", m_db.getLastError().c_str());
        return false;
    }

    LOG_INFO("用户创建成功: %s (昵称: %s)", username.c_str(), nickname.c_str());
    return true;
}

std::optional<UserModel> UserDao::findByUsername(const std::string &username)
{
    if (!m_db.isConnected()) return std::nullopt;

    std::string sql = "SELECT id, username, password_md5, nickname, created_at FROM users WHERE username = '"
        + m_db.escapeString(username) + "'";

    // query() 返回独立结果集，next() 推进到下一行
    auto rs = m_db.query(sql);
    if (!rs.next()) return std::nullopt;

    UserModel user;
    user.id = rs.getInt(0);
    user.username = rs.getString(1);
    user.password_hash = rs.getString(2);
    user.nickname = rs.getString(3);
    user.created_at = rs.getString(4);

    return user;
}

bool UserDao::existsByUsername(const std::string &username)
{
    if (!m_db.isConnected()) return false;

    std::string sql = "SELECT COUNT(*) FROM users WHERE username = '"
        + m_db.escapeString(username) + "'";

    auto rs = m_db.query(sql);
    if (!rs.next()) return false;

    return rs.getInt64(0) > 0;
}

bool UserDao::updatePasswordHash(const std::string &username, const std::string &passwordHash)
{
    if (!m_db.isConnected()) return false;

    std::string sql = "UPDATE users SET password_md5 = '"
        + m_db.escapeString(passwordHash)
        + "' WHERE username = '" + m_db.escapeString(username) + "'";

    if (!m_db.executeUpdate(sql)) {
        LOG_ERROR("更新密码哈希失败: %s", m_db.getLastError().c_str());
        return false;
    }
    return true;
}
