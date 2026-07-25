#pragma once

#include "Ipv4Endpoint.h"

#include <WinSock2.h>
#include <cstdint>

namespace Common {

    // UDP 소켓 핸들을 RAII 방식으로 관리
    class UdpSocket {
        public:
            UdpSocket() = default;
            explicit UdpSocket(SOCKET socket);
            ~UdpSocket();

            UdpSocket(const UdpSocket&) = delete;
            UdpSocket& operator=(const UdpSocket&) = delete;

            UdpSocket(UdpSocket&& other) noexcept;
            UdpSocket& operator=(UdpSocket&& other) noexcept;

            bool Create();
            bool Bind(const Ipv4Endpoint& endpoint);

            // 반환값은 실제 송신 바이트 수, SOCKET_ERROR는 오류
            int SendTo(const void* data, int size, const Ipv4Endpoint& destination);

            // 반환값은 실제 수신 바이트 수, SOCKET_ERROR는 오류
            int ReceiveFrom(void* buffer, int bufferSize, Ipv4Endpoint& sender);

            // recvfrom의 최대 대시 시간을 설정
            bool SetReceiveTimeout(uint32_t timeoutMilliseconds);

            bool IsValid() const;
            SOCKET GetNativeSocket() const;
            void Close();

            static int GetLastError();

        private:
            SOCKET socket_ = INVALID_SOCKET;
    };

} // namespace Common