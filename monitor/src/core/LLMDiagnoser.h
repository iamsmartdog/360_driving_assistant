#ifndef LLMDIAGNOSER_H
#define LLMDIAGNOSER_H

#include "AnomalyDetector.h"
#include "MonitorConfig.h"
#include <string>
#include <vector>
#include <functional>

namespace monitor {

/**
 * @brief LLM 诊断报告生成器
 * 异常触发后，将异常上下文组装为结构化 Prompt，调用 LLM API 生成诊断报告。
 *
 * 报告包含：影响评估、根因推测、排查建议。
 * 支持自定义 LLM API（兼容 OpenAI 格式）。
 */
class LLMDiagnoser {
public:
    explicit LLMDiagnoser(const MonitorConfig &config);

    // 生成诊断报告（异步，通过回调返回结果）
    // 报告内容：影响评估 → 根因推测 → 排查步骤
    void generateReport(const std::vector<Alert> &alerts,
                        const SystemSample &sysSample,
                        std::function<void(bool success, const std::string &report)> callback);

    // 是否启用
    bool enabled() const { return m_enabled; }

private:
    bool m_enabled;
    std::string m_apiUrl;
    std::string m_apiKey;
    std::string m_model;

    // 上次 LLM 调用的时间戳（毫秒），用于冷却控制
    int64_t m_lastCallMs = 0;

    // 组装 Prompt
    std::string buildPrompt(const std::vector<Alert> &alerts,
                            const SystemSample &sysSample);

    // 调用 LLM API（HTTP POST）
    std::string callLLMApi(const std::string &prompt);
};

} // namespace monitor

#endif // LLMDIAGNOSER_H