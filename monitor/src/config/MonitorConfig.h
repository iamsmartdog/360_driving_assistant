#ifndef MONITORCONFIG_H
#define MONITORCONFIG_H

#include <string>
#include <vector>

namespace monitor {

/**
 * @brief 监控目标进程配置
 * 通过进程名（可含关键字）匹配目标，如 "server"、"client" 等。
 */
struct TargetProcess {
    std::string name;          // 进程名关键字（用于 pidof / 命令行匹配）
    double cpuThreshold;       // CPU 瞬时阈值（%）
    double memThresholdMB;     // 内存瞬时阈值（MB）
    int    fdThreshold;        // 文件句柄瞬时阈值
};

/**
 * @brief 监控守护进程配置
 * 从 monitor.ini 读取，缺省使用内置默认值，保证开箱即用。
 */
struct MonitorConfig {
    // 采样周期（秒）
    int sampleIntervalSec = 2;

    // 趋势检测窗口（保留多少采样点用于线性回归）
    int trendWindowSize = 30;

    // 趋势告警阈值：内存增长率（MB/采样点）
    double memTrendThreshold = 5.0;

    // 系统级阈值
    double systemCpuThreshold = 90.0;   // 系统总CPU阈值（%）
    double systemMemThreshold = 90.0;   // 系统内存使用率阈值（%）

    // LLM 配置
    bool   llmEnabled = false;
    std::string llmApiUrl = "";
    std::string llmApiKey = "";
    std::string llmModel = "";

    // 日志文件路径（空则不写文件）
    std::string logFile = "monitor.log";

    // 监控的目标进程
    std::vector<TargetProcess> targets = {
        {"server", 80.0, 512.0, 500},
        {"client", 80.0, 512.0, 500}
    };

    // 从文件加载（简单 INI 解析）
    bool loadFromFile(const std::string &path);
};

} // namespace monitor

#endif // MONITORCONFIG_H