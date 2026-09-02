#ifndef MYSQLDAO_H
#define MYSQLDAO_H

#include <string>
#include <cstdint>
#include <mutex>
#include <mysql/mysql.h>

/**
 * @brief RAII 包装的 MySQL 结果集
 *
 * 设计目的：把查询结果从 MySqlDao 的成员状态中剥离出来，使每个查询持有独立的
 * 结果集对象。这样：
 *   1. 支持嵌套查询（一个查询循环中可以再发起另一个查询，互不冲掉结果）
 *   2. 可重入、可并发（结果集在 mysql_store_result 后已拷贝到客户端，与连接解耦）
 *   3. 析构自动 mysql_free_result，无资源泄漏
 *
 * 数值解析使用 strtol/strtoll，遇到非法字符串返回 0 而非抛异常，
 * 避免 DB 脏数据导致服务端崩溃（原先 std::stoi 会抛 std::out_of_range）。
 */
class MySqlResultSet
{
public:
    explicit MySqlResultSet(MYSQL_RES *res = nullptr) noexcept;
    ~MySqlResultSet();

    MySqlResultSet(const MySqlResultSet &) = delete;
    MySqlResultSet &operator=(const MySqlResultSet &) = delete;
    MySqlResultSet(MySqlResultSet &&other) noexcept;
    MySqlResultSet &operator=(MySqlResultSet &&other) noexcept;

    // 是否持有有效结果集
    bool valid() const noexcept { return m_res != nullptr; }

    // 取下一行；到末尾返回 false。调用后可用 getXxx(col) 读取当前行
    bool next();

    // 字段数
    int fieldCount() const noexcept;

    // 以下方法在 next() 返回 true 后调用；col 从 0 起
    bool isNull(int col) const noexcept;
    std::string getString(int col) const;        // NULL 返回空串
    int getInt(int col) const noexcept;          // NULL/非法返回 0（不抛异常）
    int64_t getInt64(int col) const noexcept;    // NULL/非法返回 0（不抛异常）

private:
    MYSQL_RES *m_res;
    MYSQL_ROW m_row = nullptr;
    unsigned long *m_lengths = nullptr;
};

/**
 * @brief MySQL 数据库访问类
 *
 * 职责：管理单条 MySQL 连接，提供查询/更新接口。
 *
 * 线程安全：所有接口内部用 m_mutex 串行化，保证同一连接不会被并发调用导致状态损坏。
 * 注意：affectedRows()/insertId() 反映的是“本线程最近一次通过本 Dao 执行的语句”的结果，
 *   单 worker 串行调用场景下完全正确；若未来引入多 worker 并发，应改为连接池，
 *   每个 worker 独占一条连接，而非共享单连接。
 *
 * 安全：字符串字段请用 escapeString 转义后拼接，可防 SQL 注入。
 *   （后续可演进为 mysql_stmt_prepare 参数绑定，进一步去样板。）
 */
class MySqlDao
{
public:
    MySqlDao();
    ~MySqlDao();

    MySqlDao(const MySqlDao &) = delete;
    MySqlDao &operator=(const MySqlDao &) = delete;

    // 连接数据库（参数由调用方从配置文件提供，不再内嵌默认凭据）
    bool connect(const std::string &host,
                 const std::string &user,
                 const std::string &password,
                 const std::string &database,
                 unsigned int port);

    // 断开连接
    void disconnect();

    // 是否已连接
    bool isConnected() const;

    // 执行 SELECT，返回独立结果集（不再保存到成员状态）
    MySqlResultSet query(const std::string &sql);

    // 执行 INSERT/UPDATE/DELETE/DDL
    bool executeUpdate(const std::string &sql);

    // 最近一次 executeUpdate 的影响行数
    my_ulonglong affectedRows() const;

    // 最近一次 INSERT 的自增 ID
    my_ulonglong insertId() const;

    // 转义字符串（防 SQL 注入）
    std::string escapeString(const std::string &input);

    // 获取最后的错误信息
    std::string getLastError() const;

private:
    MYSQL *m_mysql = nullptr;
    bool m_connected = false;
    mutable std::mutex m_mutex;
    my_ulonglong m_lastAffectedRows = 0;
    my_ulonglong m_lastInsertId = 0;
};

#endif // MYSQLDAO_H
