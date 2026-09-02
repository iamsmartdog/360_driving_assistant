#ifndef ALERTNOTIFIER_H
#define ALERTNOTIFIER_H

#include "AnomalyDetector.h"
#include <string>
#include <fstream>

namespace monitor {

/**
 * @brief 告警通知器
 * 将告警输出到终端（彩色）+ 日志文件。
 * 终端输出使用 ANSI 颜色，便于直观区分严重级别。
 */
class AlertNotifier {
public:
    explicit AlertNotifier(const std::string &logFile = "");

    // 输出告警
    void notify(const Alert &alert);

    // 输出告警醒目分隔头（含提示音）
    void printAlertHeader();

    // 输出 LLM 诊断报告
    void notifyReport(const std::string &report);

    // 输出系统状态摘要（周期性输出，非告警）
    void printStatus(const SystemSample &sysSample,
                     const std::vector<ProcSample> &procSamples);

private:
    std::ofstream m_logFile;

    // ANSI 颜色码
    static const char *colorForLevel(AlertLevel level);
    static const char *labelForLevel(AlertLevel level);

    // 写入日志文件
    void log(const std::string &text);
};

} // namespace monitor

#endif // ALERTNOTIFIER_H