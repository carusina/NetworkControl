#pragma once

#include <cstdint>
#include <string>

namespace Common {

    // 서버 설정
    struct ServerConfig {
        uint16_t TcpPort = 5000;
        uint16_t UdpPort = 6500;   // 클라이언트의 EntityControlInput을 받는 포트
    };

    // 클라이언트 설정
    struct ClientConfig {
        std::string Host = "127.0.0.1";
        uint16_t TcpPort = 5000;
        uint16_t UdpPort = 6000;       // 이 클라이언트가 EntityState를 받는 포트
        uint16_t ServerUdpPort = 6500; // 조종 입력을 보낼 서버 포트
    };

    // path의 key=value 설정 파일을 읽어 알려진 항목만 덮어씀
    // 파일이 없으면 false를 반환하고 config는 그대로 (기본값 유지)
    bool LoadServerConfig(const std::string& path, ServerConfig& config);
    bool LoadClientConfig(const std::string& path, ClientConfig& config);

} // namespace Common