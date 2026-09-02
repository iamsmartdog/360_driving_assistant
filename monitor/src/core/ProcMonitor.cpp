#include "ProcMonitor.h"

#include <fstream>
#include <sstream>
#include <dirent.h>
#include <unistd.h>
#include <cstring>
#include <algorithm>

namespace monitor {

std::string ProcMonitor::readFile(const std::string &path) {
    std::ifstream f(path);
    if (!f.is_open()) return "";
    std::stringstream ss;
    ss << f.rdbuf();
    return ss.str();
}

void ProcMonitor::readCpuTimes(unsigned long long &total,
                               unsigned long long &idle) {
    total = 0;
    idle = 0;
    std::string content = readFile("/proc/stat");
    if (content.empty()) return;

    // 首行 "cpu  user nice system idle iowait irq softirq steal ..."
    std::istringstream iss(content);
    std::string tag;
    iss >> tag;
    if (tag != "cpu") return;

    unsigned long long user, nice, system, idle_, iowait, irq, softirq, steal;
    if (!(iss >> user >> nice >> system >> idle_ >> iowait >> irq >> softirq >> steal))
        return;

    total = user + nice + system + idle_ + iowait + irq + softirq + steal;
    idle = idle_ + iowait;
}

void ProcMonitor::readMemInfo(double &totalKB, double &usedKB) {
    totalKB = 0;
    usedKB = 0;
    std::string content = readFile("/proc/meminfo");
    if (content.empty()) return;

    std::istringstream iss(content);
    std::string key;
    double value;
    std::string unit;
    double total = 0, free = 0, buffers = 0, cached = 0, sreclaimable = 0;
    while (iss >> key >> value >> unit) {
        if (key == "MemTotal:") total = value;
        else if (key == "MemFree:") free = value;
        else if (key == "Buffers:") buffers = value;
        else if (key == "Cached:") cached = value;
        else if (key == "SReclaimable:") sreclaimable = value;
    }
    totalKB = total;
    // 实际使用 = 总 - 空闲 - 缓存（Buffer/Cache 可回收）
    double used = total - free - buffers - cached - sreclaimable;
    if (used < 0) used = 0;
    usedKB = used;
}

void ProcMonitor::readLoadAvg(long &l1, long &l5, long &l15) {
    l1 = l5 = l15 = 0;
    std::string content = readFile("/proc/loadavg");
    if (content.empty()) return;
    std::istringstream iss(content);
    iss >> l1 >> l5 >> l15;
}

SystemSample ProcMonitor::sampleSystem() {
    SystemSample s;

    // CPU 使用率（基于两次采样的 delta）
    unsigned long long total, idle;
    readCpuTimes(total, idle);
    if (m_hasPrevCpu && total > m_lastCpuTotal) {
        double totalDelta = static_cast<double>(total - m_lastCpuTotal);
        double idleDelta = static_cast<double>(idle - m_lastCpuIdle);
        if (totalDelta > 0) {
            s.cpuPercent = (1.0 - idleDelta / totalDelta) * 100.0;
        }
    }
    m_lastCpuTotal = total;
    m_lastCpuIdle = idle;
    m_hasPrevCpu = true;

    // 内存
    double totalKB = 0, usedKB = 0;
    readMemInfo(totalKB, usedKB);
    s.memTotalKB = totalKB;
    s.memUsedKB = usedKB;
    if (totalKB > 0) s.memPercent = usedKB / totalKB * 100.0;

    // 负载
    readLoadAvg(s.loadAvg1, s.loadAvg5, s.loadAvg15);

    return s;
}

// 列出 /proc 下所有 PID
static std::vector<int> listPids() {
    std::vector<int> pids;
    DIR *dir = opendir("/proc");
    if (!dir) return pids;
    struct dirent *ent;
    while ((ent = readdir(dir)) != nullptr) {
        if (ent->d_type != DT_DIR) continue;
        char *end = nullptr;
        long pid = strtol(ent->d_name, &end, 10);
        if (end && *end == '\0') {
            pids.push_back(static_cast<int>(pid));
        }
    }
    closedir(dir);
    return pids;
}

// 读取进程 cmdline（用于关键字匹配）
std::string ProcMonitor::readCmdline(int pid) {
    std::string path = "/proc/" + std::to_string(pid) + "/cmdline";
    std::string content = readFile(path);
    // cmdline 各参数以 \0 分隔，转为空格
    std::replace(content.begin(), content.end(), '\0', ' ');
    return content;
}

// 读取进程名（/proc/[pid]/comm）
std::string ProcMonitor::readComm(int pid) {
    std::string path = "/proc/" + std::to_string(pid) + "/comm";
    std::string content = readFile(path);
    // 去掉末尾换行
    while (!content.empty() && (content.back() == '\n' || content.back() == '\r'))
        content.pop_back();
    return content;
}

ProcSample ProcMonitor::samplePid(int pid) {
    ProcSample s;
    s.pid = pid;
    s.name = readComm(pid);

    // 读取 /proc/[pid]/stat（字段以空格分隔，进程名含空格需特殊处理）
    std::string path = "/proc/" + std::to_string(pid) + "/stat";
    std::string content = readFile(path);
    if (content.empty()) {
        s.alive = false;
        return s;
    }

    // 找到最后一个 ')' 之后的字段（comm 可能含空格和括号）
    auto rparen = content.rfind(')');
    if (rparen == std::string::npos) {
        s.alive = false;
        return s;
    }
    std::string rest = content.substr(rparen + 1);
    std::istringstream iss(rest);

    // 字段 3(state) 起。相关字段位置（相对 state 后的偏移）：
    //   state(3) 第0个, ppid(4) 第1个, ...
    //   utime(14) 第11个, stime(15) 第12个,
    //   threads(20) 第17个, starttime(22) 第19个
    std::vector<std::string> fields;
    std::string token;
    while (iss >> token) fields.push_back(token);

    if (fields.size() >= 20) {
        s.threads = std::stol(fields[17]);
        s.upTimeSec = std::stol(fields[19]);
        // utime + stime = 进程累计 CPU 时间（jiffies）
        unsigned long long utime = std::stoull(fields[11]);
        unsigned long long stime = std::stoull(fields[12]);
        s.jiffies = utime + stime;
    }

    // 内存：/proc/[pid]/status 的 VmRSS
    std::string status = readFile("/proc/" + std::to_string(pid) + "/status");
    std::istringstream ss(status);
    std::string line;
    while (std::getline(ss, line)) {
        if (line.rfind("VmRSS:", 0) == 0) {
            std::istringstream ls(line.substr(7));
            ls >> s.memKB;
            break;
        }
    }

    // 文件句柄数：/proc/[pid]/fd 目录项数
    std::string fdDir = "/proc/" + std::to_string(pid) + "/fd";
    int fdCount = 0;
    DIR *fd = opendir(fdDir.c_str());
    if (fd) {
        struct dirent *ent;
        while ((ent = readdir(fd)) != nullptr) {
            if (strcmp(ent->d_name, ".") != 0 && strcmp(ent->d_name, "..") != 0)
                fdCount++;
        }
        closedir(fd);
    }
    s.fdCount = fdCount;
    s.alive = true;

    // 读取内存分布详情（/proc/[pid]/status）
    // 从 status 中提取 VmHeap（堆）、VmStack（栈）、缺页中断、上下文切换
    std::istringstream statusStream(status);
    std::string statusLine;
    while (std::getline(statusStream, statusLine)) {
        if (statusLine.rfind("VmHeap:", 0) == 0) {
            std::istringstream ls(statusLine.substr(7));
            ls >> s.heapKB;
        } else if (statusLine.rfind("VmStack:", 0) == 0) {
            std::istringstream ls(statusLine.substr(8));
            ls >> s.stackKB;
        } else if (statusLine.rfind("voluntary_ctxt_switches:", 0) == 0) {
            std::istringstream ls(statusLine.substr(24));
            ls >> s.voluntaryCtxSw;
        } else if (statusLine.rfind("nonvoluntary_ctxt_switches:", 0) == 0) {
            std::istringstream ls(statusLine.substr(27));
            ls >> s.nonvoluntaryCtxSw;
        }
    }
    // 缺页中断从 /proc/[pid]/stat 的第10、12字段读取
    // 已在前面读取 stat 内容，但 stat 解析复杂，从 /proc/[pid]/status 的 VmPeak 不可用
    // 从 /proc/[pid]/stat 中提取 minflt(10) 和 majflt(12)
    // 前面已解析 fields 数组，fields[7] = minflt(10), fields[9] = majflt(12)
    if (fields.size() >= 12) {
        s.pageFaultMinor = std::stoi(fields[7]);
        s.pageFaultMajor = std::stoi(fields[9]);
    }

    return s;
}

double ProcMonitor::calcCpuPercent(unsigned long long prevJiffies,
                                   unsigned long long curJiffies,
                                   unsigned long long deltaTick) {
    if (deltaTick == 0 || curJiffies < prevJiffies) return 0.0;
    double delta = static_cast<double>(curJiffies - prevJiffies);
    // 进程 CPU% = 进程消耗 jiffies / 系统总 jiffies * 100
    return (delta / static_cast<double>(deltaTick)) * 100.0;
}

std::vector<ProcSample> ProcMonitor::sampleProcess(const std::string &keyword) {
    std::vector<ProcSample> result;
    auto pids = listPids();
    for (int pid : pids) {
        ProcSample s = samplePid(pid);
        if (!s.alive) continue;

        // 匹配：进程名包含关键字，或命令行包含关键字
        bool matched = s.name.find(keyword) != std::string::npos;
        if (!matched) {
            std::string cmdline = readCmdline(pid);
            if (cmdline.find(keyword) != std::string::npos) {
                matched = true;
            }
        }
        if (matched) {
            result.push_back(s);
        }
    }
    return result;
}

} // namespace monitor