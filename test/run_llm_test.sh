#!/bin/bash
# ============================================================
# 监控守护进程 - LLM 诊断测试脚本
#
# 用法:
#   ./run_llm_test.sh              # 用默认配置测试
#   ./run_llm_test.sh <时长秒>      # 指定测试时长
#
# 功能:
#   1. 启动 cpu_stress 泄漏进程（每3秒泄漏10MB，进程名含 cpu_stress）
#   2. 启动监控守护进程（LLM 已启用）
#   3. 检测到内存泄漏后，LLM 生成诊断报告
#   4. 测试结束自动清理进程
# ============================================================

set -u

# 默认配置
DURATION=${1:-50}
TEST_DIR="$(cd "$(dirname "$0")" && pwd)"
MONITOR_BIN="${TEST_DIR}/../build/bin/monitor_daemon"
CONFIG="${TEST_DIR}/test_monitor.ini"
STRESS_BIN="${TEST_DIR}/cpu_stress"

echo "=============================================="
echo "  监控守护进程 LLM 诊断测试"
echo "=============================================="
echo "测试时长: ${DURATION}秒"
echo "测试目录: ${TEST_DIR}"

# 检查必要文件
if [ ! -f "${MONITOR_BIN}" ]; then
    echo "错误: 找不到 monitor_daemon (${MONITOR_BIN})"
    echo "请先编译: cd build && make monitor_daemon"
    exit 1
fi
if [ ! -f "${STRESS_BIN}" ]; then
    echo "错误: 找不到 cpu_stress (${STRESS_BIN})"
    echo "请先编译: g++ -o cpu_stress cpu_stress.cpp -lpthread -O2"
    exit 1
fi

# 清理可能残留的进程
echo "清理残留进程..."
pkill -f cpu_stress 2>/dev/null
sleep 1

# 启动泄漏进程（setsid 防止被终端关闭时杀掉）
echo "启动内存泄漏进程 (cpu_stress)..."
setsid "${STRESS_BIN}" --threads 0 --mem-leak 3 --mem-size 10 --duration $((DURATION + 10)) > /tmp/monitor_test_leak.log 2>&1 &
STRESS_PID=$!
sleep 2

# 确认泄漏进程在运行
if ! kill -0 "${STRESS_PID}" 2>/dev/null; then
    echo "错误: cpu_stress 启动失败，请检查 /tmp/monitor_test_leak.log"
    exit 1
fi
echo "泄漏进程已启动 (PID: ${STRESS_PID})"

# 启动监控守护进程
echo "启动监控守护进程 (LLM 已启用)..."
timeout "${DURATION}" "${MONITOR_BIN}" "${CONFIG}" 2>&1 | tee /tmp/monitor_test_result.log

echo ""
echo "=============================================="
echo "  测试完成"
echo "=============================================="
echo "监控日志: /tmp/monitor_test_result.log"
echo "泄漏日志: /tmp/monitor_test_leak.log"
echo ""
echo "LLM 诊断报告:"
grep -A80 "LLM 诊断报告" /tmp/monitor_test_result.log | head -80
echo ""
echo "告警统计:"
echo "  严重告警: $(grep -c '严重' /tmp/monitor_test_result.log)"
echo "  警告告警: $(grep -c '警告' /tmp/monitor_test_result.log)"

# 清理泄漏进程
echo ""
echo "清理测试进程..."
pkill -f cpu_stress 2>/dev/null
echo "完成。"