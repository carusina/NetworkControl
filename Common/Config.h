#pragma once

#include <cstdint>
#include <string>

namespace Common {

    // 서버 설정
    struct ServerConfig {
        uint16_t TcpPort = 5000;
    };

    // 클라이언트 설정
    struct ClientConfig {
        std::string Host = "127.0.0.1";
        uint16_t TcpPort = 5000;
        uint16_t UdpPort = 6000;
    };

    // path의 key=value 설정 파일을 읽어 알려진 항목만 덮어씀
    // 파일이 없으면 false를 반환하고 config는 그대로 (기본값 유지)
    bool LoadServerConfig(const std::string& path, ServerConfig& config);
    bool LoadClientConfig(const std::string& path, ClientConfig& config);

} // namespace Common