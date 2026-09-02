/**
 * LLM 诊断独立测试程序
 * 直接调用 LLM API，验证配置和 JSON 格式是否正确
 *
 * 用法: ./test_llm  "测试消息内容"
 */
#include <cstdio>
#include <curl/curl.h>
#include <sstream>
#include <string>

static size_t writeCallback(void *contents, size_t size, size_t nmemb, void *userp) {
    size_t total = size * nmemb;
    static_cast<std::string *>(userp)->append(static_cast<char *>(contents), total);
    return total;
}

// JSON 字符串转义
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

int main(int argc, char* argv[]) {
    std::string msg = argc > 1 ? argv[1] : "你好，请简单介绍你自己。";

    const char* url = "https://llm.xinlicloud.top/v1/chat/completions";
    const char* key = "sk-37U0drw7v3R4ltt1Czhf9bSICOBE2WwSpFw0QHsGbVNAK7P7";
    const char* model = "gpt-4o-mini";

    std::string body = R"({"model":")" + std::string(model) + R"(","messages":[{"role":"user","content":")"
                       + jsonEscape(msg) + R"("}],"temperature":0.3,"max_tokens":512})";

    printf("请求 URL: %s\n", url);
    printf("请求体: %s\n\n", body.c_str());

    CURL *curl = curl_easy_init();
    if (!curl) { printf("curl init failed\n"); return 1; }

    std::string response;
    struct curl_slist *headers = nullptr;
    headers = curl_slist_append(headers, "Content-Type: application/json");
    std::string auth = "Authorization: Bearer " + std::string(key);
    headers = curl_slist_append(headers, auth.c_str());

    curl_easy_setopt(curl, CURLOPT_URL, url);
    curl_easy_setopt(curl, CURLOPT_HTTPHEADER, headers);
    curl_easy_setopt(curl, CURLOPT_POSTFIELDS, body.c_str());
    curl_easy_setopt(curl, CURLOPT_POSTFIELDSIZE, (long)body.size());
    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, writeCallback);
    curl_easy_setopt(curl, CURLOPT_WRITEDATA, &response);
    curl_easy_setopt(curl, CURLOPT_TIMEOUT, 30L);
    curl_easy_setopt(curl, CURLOPT_SSL_VERIFYPEER, 0L);
    curl_easy_setopt(curl, CURLOPT_SSL_VERIFYHOST, 0L);

    CURLcode res = curl_easy_perform(curl);
    curl_slist_free_all(headers);
    curl_easy_cleanup(curl);

    if (res != CURLE_OK) {
        printf("CURL 错误: %s\n", curl_easy_strerror(res));
        return 1;
    }

    printf("===== 原始响应 =====\n%s\n", response.c_str());
    return 0;
}