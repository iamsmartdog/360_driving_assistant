#ifndef ANOMALYDETECTOR_H
#define ANOMALYDETECTOR_H

#include "ProcMonitor.h"
#include "MonitorConfig.h"
#include <vector>
#include <map>
#include <string>
#include <deque>
#include <mutex>

namespace monitor {

// 异常严重级别
enum class AlertLevel {
    INFO,
    WARNING,
    CRITICAL
};

// 异常类型
enum class AnomalyType {
    PROCESS_CPU_HIGH,        // 进程 CPU 瞬时过高
    PROCESS_MEM_HIGH,        // 进程内存瞬时过高
    PROCESS_FD_HIGH,         // 进程句柄数过高
    PROCESS_DIED,            // 进程意外退出
    SYSTEM_CPU_HIGH,         // 系统 CPU 过高
    SYSTEM_MEM_HIGH,         // 系统内存过高
    TREND_MEM_LEAK,          // 渐进式内存泄漏趋势
    TREND_FD_LEAK            // 渐进式句柄泄漏趋势
};

// 告警信息
struct Alert {
    AlertLevel level = AlertLevel::WARNING;
    AnomalyType type = AnomalyType::PROCESS_CPU_HIGH;
    int pid = 0;
    std::string processName;     // 进程名
    std::string description;     // 中文描述
    double currentValue = 0.0;   // 当前值
    double threshold = 0.0;      // 阈值
    int64_t timestamp = 0;       // Unix 时间戳（毫秒）
    std::string suggestion;      // 建议措施

    // 用于趋势检测的采样序列
    std::deque<double> history;  // 最近 N 个采样值
};

// 进程历史数据（用于趋势检测）
struct ProcessHistory {
    // 内存采样序列（KB）
    std::deque<double> memHistory;
    // 句柄数采样序列
    std::deque<double> fdHistory;
    // 上次采样 CPU jiffies（用于计算使用率）
    unsigned long long prevJiffies = 0;
    // 上次采样系统总 jiffies
    unsigned long long prevSysJiffies = 0;
};

/**
 * @brief 异常检测引擎
 * 双层检测：
 *   1. 瞬时检测：当前值超过阈值立即告警
 *   2. 趋势检测：对采样序列做线性回归，增长斜率超过阈值时告警（内存泄漏/句柄泄漏）
 */
class AnomalyDetector {
public:
    explicit AnomalyDetector(const MonitorConfig &config);

    // 执行一轮检测（系统 + 所有目标进程）
    // 返回本轮产生的告警（可能为空）
    std::vector<Alert> detect(const SystemSample &sysSample,
                              const std::vector<ProcSample> &procSamples,
                              unsigned long long sysTotalJiffies);

    // 清空历史数据
    void reset();

    // 获取当前历史数据快照（线程安全）
    std::map<int, ProcessHistory> getHistory() const;

private:
    const MonitorConfig &m_config;

    // 按 PID 维护历史数据
    std::map<int, ProcessHistory> m_history;
    mutable std::mutex m_mutex;

    // 瞬时检测
    Alert checkInstantaneous(const ProcSample &sample);

    // 趋势检测（线性回归判断持续增长）
    Alert checkTrend(const ProcSample &sample, ProcessHistory &history);

    // 线性回归斜率计算
    static double calcSlope(const std::deque<double> &samples);

    // 创建告警对象
    Alert makeAlert(AlertLevel level, AnomalyType type, int pid,
                    const std::string &procName, const std::string &desc,
                    double current, double threshold);
};

} // namespace monitor

#endif // ANOMALYDETECTOR_H