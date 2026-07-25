#include "Ipv4Endpoint.h"

namespace Common {

    Ipv4Endpoint::Ipv4Endpoint() {
        address_.sin_family = AF_INET;
    }

    Ipv4Endpoint Ipv4Endpoint::Any(uint16_t port)
    {
        Ipv4Endpoint endpoint;
        endpoint.address_.sin_addr.s_addr = htonl(INADDR_ANY);
        endpoint.address_.sin_port = htons(port);
        
        return endpoint;
    }

    bool Ipv4Endpoint::TryCreate(
        const std::string& ip,
        uint16_t port,
        Ipv4Endpoint& endpoint)
    {
        endpoint = Ipv4Endpoint();
        endpoint.address_.sin_port = htons(port);

        return inet_pton(AF_INET, ip.c_str(), &endpoint.address_.sin_addr) == 1;
    }

    const sockaddr_in& Ipv4Endpoint::GetNativeAddress() const {
        return address_;
    }

    sockaddr_in& Ipv4Endpoint::GetNativeAddress() {
        return address_;
    }

    uint16_t Ipv4Endpoint::GetPort() const {
        return ntohs(address_.sin_port);
    }

    std::string Ipv4Endpoint::GetIpAddress() const
    {
        char buffer[INET_ADDRSTRLEN] {};

        const char* result = inet_ntop(AF_INET, &address_.sin_addr, buffer, sizeof(buffer));

        return result != nullptr ? buffer : "";
    }

} // namespace Common