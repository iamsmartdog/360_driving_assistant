#include "MonitorConfig.h"
#include <fstream>
#include <sstream>
#include <algorithm>
#include <cctype>

namespace monitor {

// 去除前后空白
static std::string trim(const std::string &s) {
    auto start = s.find_first_not_of(" \t\r\n");
    if (start == std::string::npos) return "";
    auto end = s.find_last_not_of(" \t\r\n");
    return s.substr(start, end - start + 1);
}

bool MonitorConfig::loadFromFile(const std::string &path) {
    std::ifstream file(path);
    if (!file.is_open()) return false;

    std::string line;
    std::string section;
    while (std::getline(file, line)) {
        line = trim(line);
        if (line.empty() || line[0] == '#' || line[0] == ';') continue;

        if (line[0] == '[') {
            auto end = line.find(']');
            if (end != std::string::npos) {
                section = line.substr(1, end - 1);
            }
            continue;
        }

        auto eq = line.find('=');
        if (eq == std::string::npos) continue;

        std::string key = trim(line.substr(0, eq));
        std::string val = trim(line.substr(eq + 1));

        if (section == "General") {
            if (key == "sample_interval_sec") sampleIntervalSec = std::stoi(val);
            else if (key == "trend_window_size") trendWindowSize = std::stoi(val);
            else if (key == "mem_trend_threshold_mb") memTrendThreshold = std::stod(val);
            else if (key == "log_file") logFile = val;
        }
        else if (section == "System") {
            if (key == "cpu_threshold") systemCpuThreshold = std::stod(val);
            else if (key == "mem_threshold") systemMemThreshold = std::stod(val);
        }
        else if (section == "LLM") {
            if (key == "enabled") llmEnabled = (val == "true" || val == "1");
            else if (key == "api_url") llmApiUrl = val;
            else if (key == "api_key") llmApiKey = val;
            else if (key == "model") llmModel = val;
        }
        else if (section == "Targets") {
            if (key.find("target_") == 0) {
                // 格式: target_xxx = name,cpu_threshold,mem_threshold_mb,fd_threshold
                std::stringstream ss(val);
                std::string item;
                TargetProcess tp;
                int field = 0;
                while (std::getline(ss, item, ',')) {
                    item = trim(item);
                    switch (field) {
                        case 0: tp.name = item; break;
                        case 1: tp.cpuThreshold = std::stod(item); break;
                        case 2: tp.memThresholdMB = std::stod(item); break;
                        case 3: tp.fdThreshold = std::stoi(item); break;
                    }
                    field++;
                }
                if (!tp.name.empty()) targets.push_back(tp);
            }
        }
    }
    return true;
}

} // namespace monitor