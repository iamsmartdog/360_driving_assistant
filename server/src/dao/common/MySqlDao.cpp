#include "MySqlDao.h"
#include "ServerLogger.h"
#include <cstring>
#include <cstdlib>
#include <vector>

// ============================================================================
// MySqlResultSet 实现
// ============================================================================

MySqlResultSet::MySqlResultSet(MYSQL_RES *res) noexcept
    : m_res(res)
{
}

MySqlResultSet::~MySqlResultSet()
{
    if (m_res) {
        mysql_free_result(m_res);
    }
}

MySqlResultSet::MySqlResultSet(MySqlResultSet &&other) noexcept
    : m_res(other.m_res)
    , m_row(other.m_row)
    , m_lengths(other.m_lengths)
{
    other.m_res = nullptr;
    other.m_row = nullptr;
    other.m_lengths = nullptr;
}

MySqlResultSet &MySqlResultSet::operator=(MySqlResultSet &&other) noexcept
{
    if (this != &other) {
        if (m_res) mysql_free_result(m_res);
        m_res = other.m_res;
        m_row = other.m_row;
        m_lengths = other.m_lengths;
        other.m_res = nullptr;
        other.m_row = nullptr;
        other.m_lengths = nullptr;
    }
    return *this;
}

bool MySqlResultSet::next()
{
    if (!m_res) return false;
    m_row = mysql_fetch_row(m_res);
    if (!m_row) {
        m_lengths = nullptr;
        return false;
    }
    m_lengths = mysql_fetch_lengths(m_res);
    return true;
}

int MySqlResultSet::fieldCount() const noexcept
{
    if (!m_res) return 0;
    return static_cast<int>(mysql_num_fields(m_res));
}

bool MySqlResultSet::isNull(int col) const noexcept
{
    if (!m_row || col < 0) return true;
    return m_row[col] == nullptr;
}

std::string MySqlResultSet::getString(int col) const
{
    if (!m_row || col < 0) return "";
    const char *s = m_row[col];
    return s ? std::string(s) : std::string();
}

int MySqlResultSet::getInt(int col) const noexcept
{
    if (!m_row || col < 0) return 0;
    const char *s = m_row[col];
    if (!s) return 0;
    char *end = nullptr;
    long v = std::strtol(s, &end, 10);
    if (end == s) return 0;  // 未发生转换
    return static_cast<int>(v);
}

int64_t MySqlResultSet::getInt64(int col) const noexcept
{
    if (!m_row || col < 0) return 0;
    const char *s = m_row[col];
    if (!s) return 0;
    char *end = nullptr;
    long long v = std::strtoll(s, &end, 10);
    if (end == s) return 0;  // 未发生转换
    return static_cast<int64_t>(v);
}

// ============================================================================
// MySqlDao 实现
// ============================================================================

MySqlDao::MySqlDao()
{
    m_mysql = mysql_init(nullptr);
    if (!m_mysql) {
        LOG_ERROR("MySQL初始化失败");
    }
}

MySqlDao::~MySqlDao()
{
    disconnect();
    if (m_mysql) {
        mysql_close(m_mysql);
        m_mysql = nullptr;
    }
}

bool MySqlDao::connect(const std::string &host, const std::string &user,
                        const std::string &password, const std::string &database,
                        unsigned int port)
{
    std::lock_guard<std::mutex> lock(m_mutex);

    if (!m_mysql) return false;

    if (m_connected) {
        // 先清理旧连接
        if (m_mysql) {
            mysql_close(m_mysql);
            m_mysql = mysql_init(nullptr);
        }
        m_connected = false;
        m_lastAffectedRows = 0;
        m_lastInsertId = 0;
    }

    // 设置字符集为 UTF-8
    mysql_options(m_mysql, MYSQL_SET_CHARSET_NAME, "utf8mb4");

    if (!mysql_real_connect(m_mysql, host.c_str(), user.c_str(),
                            password.c_str(), database.c_str(), port, nullptr, 0)) {
        LOG_ERROR("MySQL连接失败: %s", mysql_error(m_mysql));
        return false;
    }

    m_connected = true;
    LOG_INFO("MySQL连接成功: %s@%s:%d/%s", user.c_str(), host.c_str(), port, database.c_str());
    return true;
}

void MySqlDao::disconnect()
{
    std::lock_guard<std::mutex> lock(m_mutex);
    if (m_connected && m_mysql) {
        mysql_close(m_mysql);
        m_mysql = mysql_init(nullptr);  // 重新初始化以备下次使用
        m_connected = false;
        m_lastAffectedRows = 0;
        m_lastInsertId = 0;
    }
}

bool MySqlDao::isConnected() const
{
    std::lock_guard<std::mutex> lock(m_mutex);
    return m_connected && (m_mysql != nullptr);
}

MySqlResultSet MySqlDao::query(const std::string &sql)
{
    std::lock_guard<std::mutex> lock(m_mutex);

    if (!m_connected) return MySqlResultSet(nullptr);

    if (mysql_query(m_mysql, sql.c_str()) != 0) {
        LOG_ERROR("SQL查询失败: %s - %s", sql.c_str(), mysql_error(m_mysql));
        return MySqlResultSet(nullptr);
    }

    // mysql_store_result 把结果集拷贝到客户端，返回后即可脱离连接独立遍历
    MYSQL_RES *res = mysql_store_result(m_mysql);
    if (!res) {
        // 查询成功但无结果集（例如 INSERT 误当 SELECT），返回空
        LOG_WARN("SQL查询无结果集: %s", sql.c_str());
        return MySqlResultSet(nullptr);
    }
    return MySqlResultSet(res);
}

bool MySqlDao::executeUpdate(const std::string &sql)
{
    std::lock_guard<std::mutex> lock(m_mutex);

    if (!m_connected) return false;

    if (mysql_query(m_mysql, sql.c_str()) != 0) {
        LOG_ERROR("SQL更新失败: %s - %s", sql.c_str(), mysql_error(m_mysql));
        return false;
    }

    // 立即捕获影响行数与自增ID，避免被后续语句覆盖
    m_lastAffectedRows = mysql_affected_rows(m_mysql);
    m_lastInsertId = mysql_insert_id(m_mysql);
    return true;
}

my_ulonglong MySqlDao::affectedRows() const
{
    std::lock_guard<std::mutex> lock(m_mutex);
    return m_lastAffectedRows;
}

my_ulonglong MySqlDao::insertId() const
{
    std::lock_guard<std::mutex> lock(m_mutex);
    return m_lastInsertId;
}

std::string MySqlDao::escapeString(const std::string &input)
{
    std::lock_guard<std::mutex> lock(m_mutex);
    if (!m_mysql) return input;

    std::vector<char> buffer(input.size() * 2 + 1);
    mysql_real_escape_string(m_mysql, buffer.data(), input.c_str(), input.size());
    return std::string(buffer.data());
}

std::string MySqlDao::getLastError() const
{
    std::lock_guard<std::mutex> lock(m_mutex);
    if (!m_mysql) return "MySQL未初始化";
    return mysql_error(m_mysql);
}
