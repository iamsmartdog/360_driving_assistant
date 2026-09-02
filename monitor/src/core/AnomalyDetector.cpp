#include "AnomalyDetector.h"
#include <cmath>
#include <ctime>

namespace monitor {

AnomalyDetector::AnomalyDetector(const MonitorConfig &config)
    : m_config(config)
{
}

Alert AnomalyDetector::makeAlert(AlertLevel level, AnomalyType type, int pid,
                                 const std::string &procName, const std::string &desc,
                                 double current, double threshold) {
    Alert a;
    a.level = level;
    a.type = type;
    a.pid = pid;
    a.processName = procName;
    a.description = desc;
    a.currentValue = current;
    a.threshold = threshold;
    a.timestamp = static_cast<int64_t>(::time(nullptr)) * 1000;
    return a;
}

double AnomalyDetector::calcSlope(const std::deque<double> &samples) {
    size_t n = samples.size();
    if (n < 3) return 0.0;

    // 简单线性回归：y = slope * x + intercept
    double sumX = 0, sumY = 0, sumXY = 0, sumXX = 0;
    for (size_t i = 0; i < n; i++) {
        double x = static_cast<double>(i);
        double y = samples[i];
        sumX += x;
        sumY += y;
        sumXY += x * y;
        sumXX += x * x;
    }
    double denom = n * sumXX - sumX * sumX;
    if (std::fabs(denom) < 1e-9) return 0.0;
    return (n * sumXY - sumX * sumY) / denom;
}

// 瞬时检测：进程级
Alert AnomalyDetector::checkInstantaneous(const ProcSample &sample) {
    // 找到该进程对应的目标配置
    const TargetProcess *target = nullptr;
    for (const auto &t : m_config.targets) {
        if (sample.name.find(t.name) != std::string::npos) {
            target = &t;
            break;
        }
    }
    // 未配置的进程不做进程级瞬时检测（但系统级仍会检测）
    if (!target) return Alert();

    Alert a;
    a.pid = sample.pid;
    a.processName = sample.name;
    a.timestamp = static_cast<int64_t>(::time(nullptr)) * 1000;

    // CPU 过高
    if (sample.cpuPercent > target->cpuThreshold) {
        a.level = AlertLevel::WARNING;
        a.type = AnomalyType::PROCESS_CPU_HIGH;
        a.description = "进程 " + sample.name + " CPU 使用率过高";
        a.currentValue = sample.cpuPercent;
        a.threshold = target->cpuThreshold;
        a.suggestion = "检查是否存在死循环或计算密集操作，可考虑降低采样频率或优化算法";
        return a;
    }

    // 内存过高
    double memMB = sample.memKB / 1024.0;
    if (memMB > target->memThresholdMB) {
        a.level = AlertLevel::WARNING;
        a.type = AnomalyType::PROCESS_MEM_HIGH;
        a.description = "进程 " + sample.name + " 内存占用过高 (总:" + std::to_string((int)memMB) + "MB"
                      + " 堆:" + std::to_string((int)(sample.heapKB / 1024.0)) + "MB"
                      + " 栈:" + std::to_string((int)(sample.stackKB / 1024.0)) + "MB)";
        a.currentValue = memMB;
        a.threshold = target->memThresholdMB;
        a.suggestion = "检查是否存在内存泄漏，建议抓取堆快照分析";
        return a;
    }

    // 句柄过高
    if (sample.fdCount > target->fdThreshold) {
        a.level = AlertLevel::WARNING;
        a.type = AnomalyType::PROCESS_FD_HIGH;
        a.description = "进程 " + sample.name + " 文件句柄数过高";
        a.currentValue = sample.fdCount;
        a.threshold = target->fdThreshold;
        a.suggestion = "检查是否存在句柄泄漏（未关闭的 fd），常见于文件/网络资源未释放";
        return a;
    }

    return Alert();  // 空告警表示正常
}

// 趋势检测：内存/句柄渐进式增长
Alert AnomalyDetector::checkTrend(const ProcSample &sample, ProcessHistory &history) {
    Alert result;  // 默认空（正常）

    // 记录内存采样
    history.memHistory.push_back(sample.memKB);
    if (static_cast<int>(history.memHistory.size()) > m_config.trendWindowSize)
        history.memHistory.pop_front();

    // 记录句柄采样
    history.fdHistory.push_back(static_cast<double>(sample.fdCount));
    if (static_cast<int>(history.fdHistory.size()) > m_config.trendWindowSize)
        history.fdHistory.pop_front();

    // 采样点足够时计算增长斜率
    if (static_cast<int>(history.memHistory.size()) >= m_config.trendWindowSize) {
        double memSlope = calcSlope(history.memHistory);  // 单位：KB/采样点
        double memSlopeMB = memSlope / 1024.0;
        if (memSlopeMB > m_config.memTrendThreshold) {
            result.level = AlertLevel::CRITICAL;
            result.type = AnomalyType::TREND_MEM_LEAK;
            result.pid = sample.pid;
            result.processName = sample.name;
            result.description = "进程 " + sample.name + " 存在渐进式内存泄漏趋势 (堆:" + std::to_string((int)(sample.heapKB / 1024.0)) + "MB)";
            result.currentValue = memSlopeMB;
            result.threshold = m_config.memTrendThreshold;
            result.suggestion = "内存持续增长，疑似泄漏。建议抓取堆转储并检查长生命周期对象的释放";
            result.timestamp = static_cast<int64_t>(::time(nullptr)) * 1000;
            return result;
        }
    }

    // 句柄泄漏趋势
    if (static_cast<int>(history.fdHistory.size()) >= m_config.trendWindowSize) {
        double fdSlope = calcSlope(history.fdHistory);  // 单位：个/采样点
        if (fdSlope > 1.0) {  // 每个采样点增加超过1个句柄
            result.level = AlertLevel::CRITICAL;
            result.type = AnomalyType::TREND_FD_LEAK;
            result.pid = sample.pid;
            result.processName = sample.name;
            result.description = "进程 " + sample.name + " 存在渐进式句柄泄漏趋势";
            result.currentValue = fdSlope;
            result.threshold = 1.0;
            result.suggestion = "文件/网络句柄持续增长，疑似泄漏。检查未关闭的资源";
            result.timestamp = static_cast<int64_t>(::time(nullptr)) * 1000;
            return result;
        }
    }

    return result;
}

std::vector<Alert> AnomalyDetector::detect(const SystemSample &sysSample,
                                           const std::vector<ProcSample> &procSamples,
                                           unsigned long long sysTotalJiffies) {
    std::lock_guard<std::mutex> lock(m_mutex);
    std::vector<Alert> alerts;

    // ===== 系统级检测 =====
    // 系统 CPU
    if (sysSample.cpuPercent > m_config.systemCpuThreshold) {
        auto a = makeAlert(AlertLevel::WARNING, AnomalyType::SYSTEM_CPU_HIGH, 0, "system",
                           "系统 CPU 使用率过高", sysSample.cpuPercent,
                           m_config.systemCpuThreshold);
        a.suggestion = "系统整体负载过高，检查是否有进程失控或资源竞争";
        alerts.push_back(a);
    }
    // 系统内存
    if (sysSample.memPercent > m_config.systemMemThreshold) {
        auto a = makeAlert(AlertLevel::WARNING, AnomalyType::SYSTEM_MEM_HIGH, 0, "system",
                           "系统内存使用率过高", sysSample.memPercent,
                           m_config.systemMemThreshold);
        a.suggestion = "系统内存吃紧，建议释放缓存或检查内存泄漏进程";
        alerts.push_back(a);
    }

    // ===== 进程级检测 =====
    for (const auto &sample : procSamples) {
        // 瞬时检测（空 description 表示正常）
        Alert inst = checkInstantaneous(sample);
        if (!inst.description.empty()) {
            alerts.push_back(inst);
        }

        // 趋势检测（需要历史）
        ProcessHistory &hist = m_history[sample.pid];
        Alert trend = checkTrend(sample, hist);
        if (!trend.description.empty()) {
            alerts.push_back(trend);
        }

        hist.prevJiffies = sample.jiffies;
        hist.prevSysJiffies = sysTotalJiffies;
    }

    return alerts;
}

void AnomalyDetector::reset() {
    std::lock_guard<std::mutex> lock(m_mutex);
    m_history.clear();
}

std::map<int, ProcessHistory> AnomalyDetector::getHistory() const {
    std::lock_guard<std::mutex> lock(m_mutex);
    return m_history;
}

} // namespace monitor