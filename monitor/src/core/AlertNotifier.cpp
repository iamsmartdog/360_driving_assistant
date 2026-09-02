#include "AlertNotifier.h"
#include <cstdio>
#include <ctime>
#include <iomanip>
#include <sstream>

namespace monitor {

// ANSI 颜色
static const char *RESET = "\033[0m";
static const char *RED = "\033[1;31m";
static const char *YELLOW = "\033[1;33m";
static const char *CYAN = "\033[1;36m";
static const char *GREEN = "\033[1;32m";
static const char *GRAY = "\033[0;90m";

AlertNotifier::AlertNotifier(const std::string &logFile) {
    if (!logFile.empty()) {
        m_logFile.open(logFile, std::ios::out | std::ios::app);
    }
}

const char *AlertNotifier::colorForLevel(AlertLevel level) {
    switch (level) {
        case AlertLevel::CRITICAL: return RED;
        case AlertLevel::WARNING: return YELLOW;
        default: return CYAN;
    }
}

const char *AlertNotifier::labelForLevel(AlertLevel level) {
    switch (level) {
        case AlertLevel::CRITICAL: return "严重";
        case AlertLevel::WARNING: return "警告";
        default: return "信息";
    }
}

// 当前时间字符串
static std::string nowStr() {
    std::time_t t = std::time(nullptr);
    std::tm tm;
    localtime_r(&t, &tm);
    char buf[32];
    std::strftime(buf, sizeof(buf), "%Y-%m-%d %H:%M:%S", &tm);
    return buf;
}

void AlertNotifier::log(const std::string &text) {
    if (m_logFile.is_open()) {
        m_logFile << "[" << nowStr() << "] " << text << std::endl;
        m_logFile.flush();
    }
}

void AlertNotifier::printAlertHeader() {
    // 清屏式醒目分隔 + 提示音
    std::printf("\n\a%s═══════════════════════════════════════════════════════%s\n",
                RED, RESET);
    std::printf("%s  ⚠  监控告警  [%s]%s\n", RED, nowStr().c_str(), RESET);
    std::printf("%s═══════════════════════════════════════════════════════%s\n",
                RED, RESET);
}

void AlertNotifier::notify(const Alert &alert) {
    // 终端彩色输出
    std::printf("%s[%s] %s%s [%s] %s%s\n",
                colorForLevel(alert.level),
                nowStr().c_str(),
                labelForLevel(alert.level),
                RESET,
                alert.processName.c_str(),
                colorForLevel(alert.level),
                alert.description.c_str());
    std::printf("%s  当前值: %.2f (阈值: %.2f)%s\n",
                GRAY, alert.currentValue, alert.threshold, RESET);
    if (!alert.suggestion.empty()) {
        std::printf("%s  建议: %s%s\n", GRAY, alert.suggestion.c_str(), RESET);
    }

    // 写入日志
    std::ostringstream oss;
    oss << "[" << labelForLevel(alert.level) << "] " << alert.description
        << " 进程=" << alert.processName << " PID=" << alert.pid
        << " 值=" << alert.currentValue << " 阈值=" << alert.threshold;
    log(oss.str());
}

void AlertNotifier::notifyReport(const std::string &report) {
    std::printf("\n%s===== LLM 诊断报告 =====%s\n", CYAN, RESET);
    std::printf("%s\n%s\n", report.c_str(), RESET);
    std::printf("%s==========================%s\n", CYAN, RESET);
    log("LLM诊断报告: " + report);
}

void AlertNotifier::printStatus(const SystemSample &sys, const std::vector<ProcSample> &procs) {
    std::string line = nowStr();
    line += " | CPU: " + std::to_string(static_cast<int>(sys.cpuPercent)) + "%"
          + " Mem: " + std::to_string(static_cast<int>(sys.memPercent)) + "%"
          + " Load: " + std::to_string(sys.loadAvg1) + "/" + std::to_string(sys.loadAvg5);

    for (const auto &p : procs) {
        line += " | " + p.name + "(" + std::to_string(p.pid) + ")"
              + " cpu=" + std::to_string(static_cast<int>(p.cpuPercent)) + "%"
              + " mem=" + std::to_string(static_cast<int>(p.memKB / 1024.0)) + "MB"
              + " fd=" + std::to_string(p.fdCount);
    }

    std::printf("\r%s%-80s", GRAY, line.c_str());
    std::fflush(stdout);
}

} // namespace monitor