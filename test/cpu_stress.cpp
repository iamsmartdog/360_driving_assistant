/**
 * CPU 压力测试程序 — 模拟异常场景（自动退出版本）
 *
 * 用法:
 *   cpu_stress                        # 默认: 4线程跑30秒
 *   cpu_stress --duration 60          # 运行60秒
 *   cpu_stress --threads 8            # 8线程跑满CPU
 *   cpu_stress --mem-leak 3 --mem-size 10  # 每3秒泄漏10MB
 *   cpu_stress --fd-leak 2            # 每2秒开一个句柄不关
 *
 * 配合监控守护进程测试:
 *   1. 编译: g++ -o cpu_stress cpu_stress.cpp -lpthread -O2
 *   2. 运行: ./cpu_stress
 *   3. 另开终端: ./monitor_daemon test_monitor.ini
 *   4. 观察告警触发
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

// ==================== 参数 ====================
static int g_threads = 4;
static int g_duration = 30;         // 运行秒数
static int g_memLeakInterval = 0;   // 秒，0表示不启用
static int g_memLeakSizeMB = 10;    // 每次泄漏大小
static int g_fdLeakInterval = 0;    // 秒，0表示不启用
static volatile bool g_running = true;

// ==================== CPU 压力线程 ====================
void* cpuBurner(void*) {
    while (g_running) {
        // 纯计算密集型：浮点运算 + 整数运算，让 CPU 跑满
        volatile double x = 3.1415926535;
        for (int i = 0; i < 1000000; i++) {
            x = x * 1.0000001 + 0.0000001;
            x = x / 1.0000001;
            x = x * x;
            x = x * 0.9999999;
        }
    }
    return nullptr;
}

// ==================== 内存泄漏线程 ====================
void* memLeaker(void*) {
    std::vector<void*> blocks;
    int count = 0;
    while (g_running) {
        sleep(g_memLeakInterval);
        size_t size = static_cast<size_t>(g_memLeakSizeMB) * 1024 * 1024;
        void* p = malloc(size);
        if (p) {
            // 写入数据防止 COW 优化
            memset(p, 0xFF, size);
            blocks.push_back(p);
            count++;
            printf("[内存泄漏] 第 %d 次: 已泄漏 %d MB (累计 %zu MB)\n",
                   count, g_memLeakSizeMB, blocks.size() * g_memLeakSizeMB);
        }
    }
    return nullptr;
}

// ==================== 句柄泄漏线程 ====================
void* fdLeaker(void*) {
    std::vector<int> fds;
    int count = 0;
    while (g_running) {
        sleep(g_fdLeakInterval);
        int fd = open("/dev/null", O_RDONLY);
        if (fd >= 0) {
            fds.push_back(fd);
            count++;
            printf("[句柄泄漏] 第 %d 次: 已打开 %d 个文件句柄\n", count, count);
        }
    }
    return nullptr;
}

// ==================== 打印帮助 ====================
void printUsage(const char* prog) {
    printf("用法: %s [选项]\n", prog);
    printf("选项:\n");
    printf("  --duration SEC    运行时长 (默认: 30秒)\n");
    printf("  --threads N       CPU 压力线程数 (默认: 4)\n");
    printf("  --mem-leak N      内存泄漏间隔 N 秒 (默认: 0=不启用)\n");
    printf("  --mem-size MB     每次内存泄漏大小 MB (默认: 10)\n");
    printf("  --fd-leak N       句柄泄漏间隔 N 秒 (默认: 0=不启用)\n");
    printf("  --help            显示此帮助\n");
    printf("\n示例:\n");
    printf("  %s                               # CPU 4线程跑30秒\n", prog);
    printf("  %s --threads 8 --duration 60     # 8线程跑60秒\n", prog);
    printf("  %s --mem-leak 3 --mem-size 20    # 每3秒泄漏20MB\n", prog);
    printf("  %s --fd-leak 2                   # 每2秒泄漏一个句柄\n", prog);
}

// ==================== 主函数 ====================
int main(int argc, char* argv[]) {
    // 解析参数
    for (int i = 1; i < argc; i++) {
        std::string arg = argv[i];
        if (arg == "--help") {
            printUsage(argv[0]);
            return 0;
        } else if (arg == "--duration" && i + 1 < argc) {
            g_duration = std::stoi(argv[++i]);
        } else if (arg == "--threads" && i + 1 < argc) {
            g_threads = std::stoi(argv[++i]);
        } else if (arg == "--mem-leak" && i + 1 < argc) {
            g_memLeakInterval = std::stoi(argv[++i]);
        } else if (arg == "--mem-size" && i + 1 < argc) {
            g_memLeakSizeMB = std::stoi(argv[++i]);
        } else if (arg == "--fd-leak" && i + 1 < argc) {
            g_fdLeakInterval = std::stoi(argv[++i]);
        } else {
            printf("未知参数: %s\n", arg.c_str());
            printUsage(argv[0]);
            return 1;
        }
    }

    printf("========================================\n");
    printf("  360° 监控守护进程 - 压力测试程序\n");
    printf("========================================\n");
    printf("运行时长: %d 秒\n", g_duration);
    printf("CPU 压力线程: %d\n", g_threads);
    if (g_memLeakInterval > 0) {
        printf("内存泄漏: 每 %d 秒泄漏 %d MB\n", g_memLeakInterval, g_memLeakSizeMB);
    } else {
        printf("内存泄漏: 未启用\n");
    }
    if (g_fdLeakInterval > 0) {
        printf("句柄泄漏: 每 %d 秒打开一个文件句柄\n", g_fdLeakInterval);
    } else {
        printf("句柄泄漏: 未启用\n");
    }
    printf("----------------------------------------\n");
    printf("进程名: cpu_stress\n");
    printf("PID: %d\n", getpid());
    printf("========================================\n\n");

    // 创建 CPU 压力线程
    std::vector<pthread_t> cpuThreads(g_threads);
    for (int i = 0; i < g_threads; i++) {
        pthread_create(&cpuThreads[i], nullptr, cpuBurner, nullptr);
    }

    // 创建内存泄漏线程
    pthread_t memThread = 0;
    if (g_memLeakInterval > 0) {
        pthread_create(&memThread, nullptr, memLeaker, nullptr);
    }

    // 创建句柄泄漏线程
    pthread_t fdThread = 0;
    if (g_fdLeakInterval > 0) {
        pthread_create(&fdThread, nullptr, fdLeaker, nullptr);
    }

    // 倒计时运行
    time_t start = time(nullptr);
    while (g_running) {
        sleep(1);
        int elapsed = static_cast<int>(time(nullptr) - start);
        if (elapsed >= g_duration) {
            g_running = false;
        }
        // 每5秒打印一次状态
        if (elapsed % 5 == 0 && elapsed > 0) {
            printf("[运行中] %d/%d 秒\n", elapsed, g_duration);
        }
    }

    // 停止所有线程
    g_running = false;

    // 等待 CPU 线程结束
    for (int i = 0; i < g_threads; i++) {
        pthread_join(cpuThreads[i], nullptr);
    }

    printf("\n测试完成！共运行 %d 秒。\n", g_duration);
    return 0;
}