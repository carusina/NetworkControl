#pragma once

#include <cstdint>
#include <string>

#include <WinSock2.h>
#include <WS2tcpip.h>

namespace Common {

    // Ipv4 주소와 포트를 표현
    class Ipv4Endpoint {
        public:
            Ipv4Endpoint();

            static Ipv4Endpoint Any(uint16_t port);
            static bool TryCreate(const std::string& ip, uint16_t port, Ipv4Endpoint& endpoint);

            const sockaddr_in& GetNativeAddress() const;
            sockaddr_in& GetNativeAddress();

            uint16_t GetPort() const;
            std::string GetIpAddress() const;

        private:
            sockaddr_in address_{};
    };

} // namespace Common