#include "Config.h"

#include <fstream>
#include <unordered_map>

namespace Common {

namespace {

    std::string Trim(const std::string& text)
    {
        const size_t first = text.find_first_not_of(" \t\r\n");
        if (first == std::string::npos) {
            return "";
        }

        const size_t last = text.find_last_not_of(" \t\r\n");
        return text.substr(first, last - first + 1);
    }

    // "key=value" 한 줄씩 읽음, '#'/';'로 시작하는 줄과 빈 줄은 무시
    bool TryReadKeyValues(const std::string& path, std::unordered_map<std::string, std::string>& values)
    {
        std::ifstream file(path);
        if (!file.is_open()) {
            return false;
        }

        std::string line;
        while (std::getline(file, line))
        {
            const std::string trimmed = Trim(line);

            if (trimmed.empty() || trimmed[0] == '#' || trimmed[0] == ';') {
                continue;
            }

            const size_t separatorPos = trimmed.find('=');
            if (separatorPos == std::string::npos) {
                continue;
            }

            const std::string key = Trim(trimmed.substr(0, separatorPos));
            const std::string value = Trim(trimmed.substr(separatorPos + 1));

            if (!key.empty()) {
                values[key] = value;
            }
        }

        return true;
    }

    // 잘못된 값은 무시하고 기존 포트를 그대로 둠
    void ApplyPort(const std::unordered_map<std::string, std::string>& values, const char* key, uint16_t& port)
    {
        const auto it = values.find(key);
        if (it == values.end()) {
            return;
        }

        try
        {
            const int parsed = std::stoi(it->second);
            if (parsed > 0 && parsed <= 65535) {
                port = static_cast<uint16_t>(parsed);
            }
        }
        catch (const std::exception&) { }
    }

} // namespace

bool LoadServerConfig(const std::string& path, ServerConfig& config)
{
    std::unordered_map<std::string, std::string> values;
    if (!TryReadKeyValues(path, values)) {
        return false;
    }

    ApplyPort(values, "TcpPort", config.TcpPort);
    return true;
}

bool LoadClientConfig(const std::string& path, ClientConfig& config)
{
    std::unordered_map<std::string, std::string> values;
    if (!TryReadKeyValues(path, values)) {
        return false;
    }

    const auto hostIt = values.find("Host");
    if (hostIt != values.end() && !hostIt->second.empty()) {
        config.Host = hostIt->second;
    }

    ApplyPort(values, "TcpPort", config.TcpPort);
    ApplyPort(values, "UdpPort", config.UdpPort);

    return true;
}

} // namespace Common
