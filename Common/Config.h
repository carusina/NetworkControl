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
    
} // namespace Common