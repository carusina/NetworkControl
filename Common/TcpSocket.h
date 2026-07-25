#pragma once

#include "Ipv4Endpoint.h"

#include <cstddef>
#include <WinSock2.h>

namespace Common {

    // TCP 소켓 핸들을 RAII 방식으로 관리
    class TcpSocket {
        public:
            TcpSocket() = default;
            explicit TcpSocket(SOCKET socket);
            ~TcpSocket();

            TcpSocket(const TcpSocket&) = delete;
            TcpSocket& operator=(const TcpSocket&) = delete;

            TcpSocket(TcpSocket&& other) noexcept;
            TcpSocket& operator=(TcpSocket&& other) noexcept;

            bool Create();
            bool Bind(const Ipv4Endpoint& endpoint);
            bool Listen(int backlog);
            bool Connect(const Ipv4Endpoint& endpoint);

            TcpSocket Accept(Ipv4Endpoint* remoteEndpoint = nullptr);

            // 모든 바이트가 전송될 때까지 TCP 송신을 반복
            bool SendAll(const void* data, size_t size);

            // 0은 연결 종료, SOCKET_ERROR는 수신 오류
            int Receive(void* buffer, int bufferSize);

            // 지정한 크기만큼 수신할 때까지 TCP 수신을 반복
            bool ReceiveAll(void* data, size_t size);

            bool IsValid() const;
            SOCKET GetNativeSocket() const;
            void Close();

            static int GetLastError();

        private:
            SOCKET socket_ = INVALID_SOCKET;
    };

} // namespace Common 