/**
 * 服务端故障注入程序 — 模拟 server 进程发生内存/句柄泄漏 + CPU 飙升
 *
 * 这个程序的进程名刻意命名为 "server"，与 360 服务端进程同名，
 * 用于验证监控守护进程能识别"项目自身的服务进程"出现故障。
 *
 * 用法:
 *   server_fault --duration 40 --mem-leak 5 --mem-size 20 --cpu-spike
 *
 * 关键点：
 *   - 进程名包含 "server"，会命中 monitor.ini 中 [Targets] 的 server 配置
 *   - 模拟真实业务进程的内存泄漏、句柄泄漏、CPU 异常
 */

#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <unistd.h>
#include <pthread.h>
#include <vector>
#include <string>
#include <fcntl.h>
#include <ctime>

static int g_duration = 40;
static int g_memLeakInterval = 5;   // 每N秒泄漏一次
static int g_memLeakSizeMB = 20;    // 每次泄漏大小
static int g_fdLeakInterval = 3;    // 每N秒泄漏一个句柄
static bool g_cpuSpike = true;      // 是否 CPU 飙升
static volatile bool g_running = true;

// CPU 飙升线程
void* cpuBurner(void*) {
    while (g_running) {
        volatile double x = 3.1415926535;
        for (int i = 0; i < 2000000; i++) {
            x = x * 1.0000001 + 0.0000001;
            x = x / 1.0000001;
            x = x * x;
        }
    }
    return nullptr;
}

// 内存泄漏线程
void* memLeaker(void*) {
    std::vector<void*> blocks;
    int count = 0;
    while (g_running) {
        sleep(g_memLeakInterval);
        size_t size = static_cast<size_t>(g_memLeakSizeMB) * 1024 * 1024;
        void* p = malloc(size);
        if (p) {
            memset(p, 0xFF, size);
            blocks.push_back(p);
            count++;
            printf("[server 故障] 内存泄漏第 %d 次: +%dMB (累计 %zuMB)\n",
                   count, g_memLeakSizeMB, blocks.size() * g_memLeakSizeMB);
        }
    }
    return nullptr;
}

// 句柄泄漏线程
void* fdLeaker(void*) {
    std::vector<int> fds;
    int count = 0;
    while (g_running) {
        sleep(g_fdLeakInterval);
        int fd = open("/dev/null", O_RDONLY);
        if (fd >= 0) {
            fds.push_back(fd);
            count++;
            printf("[server 故障] 句柄泄漏第 %d 次: 已打开 %d 个句柄\n", count, count);
        }
    }
    return nullptr;
}

int main(int argc, char* argv[]) {
    for (int i = 1; i < argc; i++) {
        std::string arg = argv[i];
        if (arg == "--duration" && i + 1 < argc) g_duration = std::stoi(argv[++i]);
        else if (arg == "--mem-leak" && i + 1 < argc) g_memLeakInterval = std::stoi(argv[++i]);
        else if (arg == "--mem-size" && i + 1 < argc) g_memLeakSizeMB = std::stoi(argv[++i]);
        else if (arg == "--fd-leak" && i + 1 < argc) g_fdLeakInterval = std::stoi(argv[++i]);
        else if (arg == "--no-cpu") g_cpuSpike = false;
        else if (arg == "--help") {
            printf("用法: %s [选项]\n", argv[0]);
            printf("  --duration SEC   运行时长 (默认40)\n");
            printf("  --mem-leak N     内存泄漏间隔秒 (默认5)\n");
            printf("  --mem-size MB    每次泄漏大小MB (默认20)\n");
            printf("  --fd-leak N      句柄泄漏间隔秒 (默认3)\n");
            printf("  --no-cpu         不模拟CPU飙升\n");
            return 0;
        }
    }

    printf("==========================================\n");
    printf("  360服务端故障注入测试程序 (进程名=server)\n");
    printf("==========================================\n");
    printf("运行时长: %d秒 | 进程名: server | PID: %d\n", g_duration, getpid());
    printf("内存泄漏: 每%d秒+%dMB | 句柄泄漏: 每%d秒+1 | CPU飙升: %s\n",
           g_memLeakInterval, g_memLeakSizeMB, g_fdLeakInterval,
           g_cpuSpike ? "开" : "关");
    printf("启动后请运行监控守护进程观察告警\n\n");

    std::vector<pthread_t> cpuThreads;
    if (g_cpuSpike) {
        cpuThreads.resize(4);
        for (auto& t : cpuThreads) pthread_create(&t, nullptr, cpuBurner, nullptr);
    }

    pthread_t memThread, fdThread;
    pthread_create(&memThread, nullptr, memLeaker, nullptr);
    pthread_create(&fdThread, nullptr, fdLeaker, nullptr);

    time_t start = time(nullptr);
    while (g_running) {
        sleep(1);
        if (static_cast<int>(time(nullptr) - start) >= g_duration) g_running = false;
    }

    for (auto& t : cpuThreads) pthread_join(t, nullptr);
    printf("\n[server] 故障注入完成，进程即将退出。\n");
    return 0;
}