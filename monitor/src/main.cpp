#include "ProcMonitor.h"
#include "AnomalyDetector.h"
#ifdef MONITOR_ENABLE_LLM
#include "LLMDiagnoser.h"
#endif
#include "AlertNotifier.h"
#include "MonitorConfig.h"

#include <csignal>
#include <cstdlib>
#include <unistd.h>
#include <cstdio>
#include <chrono>
#include <thread>

using namespace monitor;

// ============================================================================
// 360° 智能行车辅助系统 - 监控守护进程
//
// 独立于客户端和服务端运行的系统监控中间件，负责：
//   1. 周期性采集系统/进程指标（/proc）
//   2. 双层异常检测（瞬时阈值 + 渐进趋势）
//   3. 异常时输出彩色终端告警 + 日志
//   4. 可选 LLM 诊断报告生成
//
// 启动方式：./monitor_daemon [monitor.ini]
// ============================================================================

static volatile sig_atomic_t g_running = 1;

static void signalHandler(int /*signum*/) {
    g_running = 0;
}

static void printBanner() {
    std::printf("\033[1;36m");
    std::printf("╔══════════════════════════════════════════════════╗\n");
    std::printf("║     360° 智能行车辅助系统 - 监控守护进程         ║\n");
    std::printf("║     System Monitor Daemon                       ║\n");
    std::printf("╚══════════════════════════════════════════════════╝\n");
    std::printf("\033[0m\n");
}

int main(int argc, char *argv[]) {
    // 信号处理
    signal(SIGINT, signalHandler);
    signal(SIGTERM, signalHandler);

    printBanner();

    // 1. 加载配置
    MonitorConfig config;
    std::string configPath = (argc > 1) ? argv[1] : "monitor.ini";
    if (config.loadFromFile(configPath)) {
        std::printf("配置已加载: %s\n", configPath.c_str());
    } else {
        std::printf("使用默认配置（未找到 %s，可创建模板文件）\n", configPath.c_str());
    }

    if (config.llmEnabled) {
        std::printf("LLM 诊断: 已启用 (API: %s)\n", config.llmApiUrl.c_str());
    } else {
        std::printf("LLM 诊断: 未启用\n");
    }

    std::printf("采样间隔: %d秒 | 趋势窗口: %d个采样点\n",
                config.sampleIntervalSec, config.trendWindowSize);
    std::printf("监控目标进程: ");
    for (const auto &t : config.targets) {
        std::printf("%s ", t.name.c_str());
    }
    std::printf("\n\n");

    // 2. 初始化组件
    ProcMonitor monitor;
    AnomalyDetector detector(config);
#ifdef MONITOR_ENABLE_LLM
    LLMDiagnoser diagnoser(config);
#endif
    AlertNotifier notifier(config.logFile);

    // 3. 主循环
    std::printf("监控已启动，按 Ctrl+C 停止\n");
    std::printf("────────────────────────────────────────────────────\n\n");

    while (g_running) {
        auto start = std::chrono::steady_clock::now();

        // 3.1 采样系统指标
        SystemSample sysSample = monitor.sampleSystem();

        // 3.2 采样目标进程指标
        std::vector<ProcSample> allProcSamples;
        for (const auto &target : config.targets) {
            auto samples = monitor.sampleProcess(target.name);
            allProcSamples.insert(allProcSamples.end(), samples.begin(), samples.end());
        }

        // 3.3 CPU 计算（基于两次采样 delta）
        unsigned long long sysTotal, sysIdle;
        ProcMonitor::readCpuTimes(sysTotal, sysIdle);

        // 为每个进程计算 CPU 使用率
        auto history = detector.getHistory();
        for (auto &sample : allProcSamples) {
            auto it = history.find(sample.pid);
            if (it != history.end() && it->second.prevJiffies > 0) {
                // 系统两次采样间的总 jiffies 增量
                unsigned long long sysDelta =
                    sysTotal > it->second.prevSysJiffies
                        ? (sysTotal - it->second.prevSysJiffies)
                        : 1;
                sample.cpuPercent = ProcMonitor::calcCpuPercent(
                    it->second.prevJiffies, sample.jiffies, sysDelta);
            }
        }

        // 3.4 异常检测
        auto alerts = detector.detect(sysSample, allProcSamples, sysTotal);

        // 3.5 仅在出现异常时输出告警 + 诊断（安静模式，不刷屏）
        if (!alerts.empty()) {
            // 醒目分隔线 + 提示音
            notifier.printAlertHeader();
            for (const auto &alert : alerts) {
                notifier.notify(alert);
            }
            // LLM 诊断（异常时触发，同步调用）
#ifdef MONITOR_ENABLE_LLM
            std::string report;
            bool success = false;
            diagnoser.generateReport(alerts, sysSample,
                [&success, &report](bool s, const std::string &r) {
                    success = s;
                    report = r;
                });
            if (!report.empty()) {
                notifier.notifyReport(report);
            }
#endif
        }

        // 计算剩余等待时间，保证固定采样间隔
        auto elapsed = std::chrono::steady_clock::now() - start;
        auto sleepMs = std::chrono::milliseconds(config.sampleIntervalSec * 1000);
        if (elapsed < sleepMs) {
            std::this_thread::sleep_for(sleepMs - elapsed);
        }
    }

    std::printf("\n\n监控守护进程已退出。\n");
    return 0;
}