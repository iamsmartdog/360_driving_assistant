#include "LLMDiagnoser.h"
#include <curl/curl.h>
#include <sstream>
#include <thread>
#include <cstring>
#include <chrono>

namespace monitor {

// CURL 写回调
static size_t writeCallback(void *contents, size_t size, size_t nmemb, void *userp) {
    size_t total = size * nmemb;
    static_cast<std::string *>(userp)->append(static_cast<char *>(contents), total);
    return total;
}

// 从 LLM API 响应 JSON 中提取 content 字段
// 简单解析：查找 "choices":[{"message":{"content":"..." 模式
static std::string extractContent(const std::string &json) {
    // 查找 "content":" 后的内容
    std::string key = "\"content\":\"";
    auto pos = json.find(key);
    if (pos == std::string::npos) return json;  // 找不到，返回原始 JSON

    pos += key.size();
    std::string content;
    bool escaped = false;
    for (size_t i = pos; i < json.size(); i++) {
        char c = json[i];
        if (escaped) {
            if (c == 'n') content += '\n';
            else if (c == 't') content += '\t';
            else if (c == 'r') content += '\r';
            else if (c == '"') content += '"';
            else if (c == '\\') content += '\\';
            else { content += '\\'; content += c; }
            escaped = false;
        } else if (c == '\\') {
            escaped = true;
        } else if (c == '"') {
            break;  // content 结束
        } else {
            content += c;
        }
    }
    return content.empty() ? json : content;
}

LLMDiagnoser::LLMDiagnoser(const MonitorConfig &config)
    : m_enabled(config.llmEnabled)
    , m_apiUrl(config.llmApiUrl)
    , m_apiKey(config.llmApiKey)
    , m_model(config.llmModel)
{
}

std::string LLMDiagnoser::buildPrompt(const std::vector<Alert> &alerts,
                                      const SystemSample &sysSample) {
    std::ostringstream prompt;

    prompt << "你是一个嵌入式Linux系统诊断专家。请根据以下系统监控数据，对当前异常进行分析。\n\n";
    prompt << "=== 系统状态 ===\n";
    prompt << "系统CPU使用率: " << sysSample.cpuPercent << "%\n";
    prompt << "系统内存使用率: " << sysSample.memPercent << "% (" << (sysSample.memUsedKB / 1024.0)
           << "MB / " << (sysSample.memTotalKB / 1024.0) << "MB)\n";
    prompt << "系统负载: " << sysSample.loadAvg1 << "/" << sysSample.loadAvg5 << "/" << sysSample.loadAvg15 << "\n\n";

    prompt << "=== 检测到的异常 ===\n";
    for (size_t i = 0; i < alerts.size(); i++) {
        const auto &a = alerts[i];
        prompt << "异常" << (i + 1) << ": " << a.description << "\n";
        prompt << "  级别: " << (a.level == AlertLevel::CRITICAL ? "严重" :
                                 a.level == AlertLevel::WARNING ? "警告" : "信息") << "\n";
        prompt << "  进程: " << a.processName << " (PID=" << a.pid << ")\n";
        prompt << "  当前值: " << a.currentValue << " (阈值: " << a.threshold << ")\n";
        prompt << "  建议: " << a.suggestion << "\n\n";
    }

    prompt << "=== 请输出结构化诊断报告，包含以下内容 ===\n";
    prompt << "1. 影响评估：异常对系统稳定性和功能的影响程度\n";
    prompt << "2. 根因推测：基于数据推测可能的根本原因\n";
    prompt << "3. 排查步骤：给出具体可操作的排查和修复步骤\n";
    prompt << "4. 预防措施：如何避免类似问题再次发生\n\n";
    prompt << "请使用中文回答，保持专业、简洁。";

    return prompt.str();
}

// JSON 字符串转义（替换特殊字符）
static std::string jsonEscape(const std::string &s) {
    std::string out;
    out.reserve(s.size() + 32);
    for (char c : s) {
        switch (c) {
            case '"':  out += "\\\""; break;
            case '\\': out += "\\\\"; break;
            case '\n': out += "\\n";  break;
            case '\r': out += "\\r";  break;
            case '\t': out += "\\t";  break;
            default:   out += c;      break;
        }
    }
    return out;
}

std::string LLMDiagnoser::callLLMApi(const std::string &prompt) {
    if (m_apiUrl.empty()) return "LLM API 未配置";

    CURL *curl = curl_easy_init();
    if (!curl) return "CURL 初始化失败";

    std::string response;

    // 构造请求体（兼容 OpenAI API 格式）
    std::string model = m_model.empty() ? "deepseek-chat" : m_model;
    std::string escapedPrompt = jsonEscape(prompt);
    std::string body = R"({"model":")" + model + R"(","messages":[{"role":"user","content":")"
                       + escapedPrompt + R"("}],"temperature":0.3,"max_tokens":2048})";

    struct curl_slist *headers = nullptr;
    headers = curl_slist_append(headers, "Content-Type: application/json");
    if (!m_apiKey.empty()) {
        std::string auth = "Authorization: Bearer " + m_apiKey;
        headers = curl_slist_append(headers, auth.c_str());
    }

    curl_easy_setopt(curl, CURLOPT_URL, m_apiUrl.c_str());
    curl_easy_setopt(curl, CURLOPT_HTTPHEADER, headers);
    curl_easy_setopt(curl, CURLOPT_POSTFIELDS, body.c_str());
    curl_easy_setopt(curl, CURLOPT_POSTFIELDSIZE, static_cast<long>(body.size()));
    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, writeCallback);
    curl_easy_setopt(curl, CURLOPT_WRITEDATA, &response);
    curl_easy_setopt(curl, CURLOPT_TIMEOUT, 30L);
    curl_easy_setopt(curl, CURLOPT_SSL_VERIFYPEER, 0L);  // 方便内网部署
    curl_easy_setopt(curl, CURLOPT_SSL_VERIFYHOST, 0L);

    CURLcode res = curl_easy_perform(curl);
    curl_slist_free_all(headers);
    curl_easy_cleanup(curl);

    if (res != CURLE_OK) {
        return "LLM API 调用失败: " + std::string(curl_easy_strerror(res));
    }

    // 提取 LLM 生成的内容（而非原始 JSON）
    return extractContent(response);
}

void LLMDiagnoser::generateReport(const std::vector<Alert> &alerts,
                                  const SystemSample &sysSample,
                                  std::function<void(bool, const std::string &)> callback) {
    if (!m_enabled || alerts.empty()) {
        if (callback) callback(false, "LLM 诊断未启用或无异常");
        return;
    }

    // 冷却控制：30 秒内最多调用一次 LLM，避免频繁请求
    auto now = std::chrono::steady_clock::now();
    auto nowMs = std::chrono::duration_cast<std::chrono::milliseconds>(now.time_since_epoch()).count();
    if (nowMs - m_lastCallMs < 30000) {
        if (callback) callback(false, "冷却中，跳过 LLM 调用");
        return;  // 冷却中，跳过
    }
    m_lastCallMs = nowMs;

    // 同步调用 LLM（阻塞直到完成，确保结果在进程退出前返回）
    std::string prompt = buildPrompt(alerts, sysSample);
    std::string result = callLLMApi(prompt);
    bool success = !result.empty() && result.find("失败") == std::string::npos
                   && result.find("error") == std::string::npos;
    if (callback) callback(success, result);
}

} // namespace monitor