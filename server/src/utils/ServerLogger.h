#ifndef SERVER_LOGGER_H
#define SERVER_LOGGER_H

#include <cstdio>
#include <cstdarg>
#include <ctime>
#include <mutex>

// ============================================================================
// ServerLogger - 服务端轻量日志（leveled + timestamped + thread-safe）
//
// 替代原先散落各处的 printf / fprintf(stderr,...)，提供统一的：
//   - 日志级别：INFO / WARN / ERROR
//   - 时间戳：[2026-08-06 12:34:56]
//   - 路由：INFO → stdout，WARN/ERROR → stderr
//   - 线程安全：std::mutex 串行化多线程并发输出，避免行交错
//
// 用法（printf 风格，迁移成本低）：
//   LOG_INFO("users表就绪");
//   LOG_ERROR("数据库连接失败: %s@%s", user.c_str(), host.c_str());
//
// 说明：
//   - 保留 printf 风格而非流式，是为了与历史代码最小改动对齐；
//     std::string 参数仍需 .c_str()。
//   - 如需文件输出/滚动/级别过滤，可在此基础上演进为 spdlog 等。
// ============================================================================

namespace server_log {

enum class Level { Info, Warn, Error };

inline void log(Level level, const char *fmt, ...)
{
    time_t now = std::time(nullptr);
    struct tm tm;
    localtime_r(&now, &tm);
    char ts[20];
    std::strftime(ts, sizeof(ts), "%Y-%m-%d %H:%M:%S", &tm);

    const char *tag = (level == Level::Info) ? "INFO "
                    : (level == Level::Warn) ? "WARN " : "ERROR";
    FILE *out = (level == Level::Info) ? stdout : stderr;

    va_list ap;
    va_start(ap, fmt);
    // 串行化并发日志，避免多线程输出交错
    static std::mutex ioMutex;
    std::lock_guard<std::mutex> lock(ioMutex);
    std::fprintf(out, "[%s] [%s] ", ts, tag);
    std::vfprintf(out, fmt, ap);
    std::fputc('\n', out);
    va_end(ap);
}

} // namespace server_log

#define LOG_INFO(fmt, ...)  server_log::log(server_log::Level::Info,  fmt, ##__VA_ARGS__)
#define LOG_WARN(fmt, ...)  server_log::log(server_log::Level::Warn,  fmt, ##__VA_ARGS__)
#define LOG_ERROR(fmt, ...) server_log::log(server_log::Level::Error, fmt, ##__VA_ARGS__)

#endif // SERVER_LOGGER_H
