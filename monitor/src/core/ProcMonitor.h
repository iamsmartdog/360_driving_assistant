#ifndef PROCMONITOR_H
#define PROCMONITOR_H

#include <string>
#include <vector>
#include <map>

namespace monitor {

// 单次采样到的进程指标
struct ProcSample {
    int pid = 0;
    std::string name;          // 进程名
    double cpuPercent = 0.0;   // CPU 使用率（%）
    double memKB = 0.0;        // 物理内存（KB）
    long threads = 0;          // 线程数
    int fdCount = 0;           // 打开的文件句柄数
    long upTimeSec = 0;        // 进程启动时长（秒）
    bool alive = false;        // 进程是否存活

    // 原始 CPU 时间（jiffies），用于两次采样间计算使用率
    unsigned long long jiffies = 0;

    // 内存分布详情（从 /proc/[pid]/status 和 smaps 获取）
    double heapKB = 0.0;       // 堆内存（kB）
    double stackKB = 0.0;      // 栈内存（kB）
    int pageFaultMajor = 0;    // 主缺页中断次数
    int pageFaultMinor = 0;    // 次缺页中断次数
    int voluntaryCtxSw = 0;    // 自愿上下文切换
    int nonvoluntaryCtxSw = 0; // 非自愿上下文切换
};

// 系统整体指标
struct SystemSample {
    double cpuPercent = 0.0;   // 总CPU使用率（%）
    double memTotalKB = 0.0;   // 总物理内存（KB）
    double memUsedKB = 0.0;    // 已用物理内存（KB）
    double memPercent = 0.0;   // 内存使用率（%）
    long loadAvg1 = 0;         // 1分钟负载
    long loadAvg5 = 0;         // 5分钟负载
    long loadAvg15 = 0;        // 15分钟负载
};

/**
 * @brief /proc 采样引擎
 * 从 /proc 文件系统读取系统与进程多维指标，为异常检测提供数据。
 *
 * 纯 Linux 实现，不依赖外部工具；CPU 计算基于两次采样之间的 delta 值。
 */
class ProcMonitor {
public:
    // 采样系统整体指标（内部处理 CPU delta）
    SystemSample sampleSystem();

    // 按进程名关键字查找并采样（返回匹配到的所有进程，去重）
    // 匹配规则：进程名包含关键字，或 /proc/[pid]/cmdline 包含关键字
    std::vector<ProcSample> sampleProcess(const std::string &keyword);

    // 采样指定 PID
    ProcSample samplePid(int pid);

    // 获取系统总 CPU 时间（用户态+内核态+空闲），用于两次采样间计算 delta
    static void readCpuTimes(unsigned long long &total,
                             unsigned long long &idle);

    // 计算进程 CPU 使用率（%）：
    //   prevJiffies/curJiffies 为两次采样间进程累计 jiffies，deltaTick 为两次采样间隔（jiffies）
    static double calcCpuPercent(unsigned long long prevJiffies,
                                 unsigned long long curJiffies,
                                 unsigned long long deltaTick);

private:
    // 上一次 CPU 采样（用于计算使用率 delta）
    unsigned long long m_lastCpuTotal = 0;
    unsigned long long m_lastCpuIdle = 0;
    bool m_hasPrevCpu = false;

    // 解析 /proc/meminfo
    static void readMemInfo(double &totalKB, double &usedKB);

    // 读取单行文件内容
    static std::string readFile(const std::string &path);

    // 读取 /proc/loadavg
    static void readLoadAvg(long &l1, long &l5, long &l15);

    // 读取进程名（/proc/[pid]/comm）
    static std::string readComm(int pid);

    // 读取进程 cmdline（用于关键字匹配）
    static std::string readCmdline(int pid);
};

} // namespace monitor

#endif // PROCMONITOR_H