#include "TcpSocket.h"

#include <limits>

namespace Common {

    TcpSocket::TcpSocket(SOCKET socket)
        : socket_(socket) {}
    
    TcpSocket::~TcpSocket() {
        Close();
    }

    TcpSocket::TcpSocket(TcpSocket&& other) noexcept
        : socket_(other.socket_)
    {
        other.socket_ = INVALID_SOCKET;
    }

    TcpSocket& TcpSocket::operator=(TcpSocket&& other) noexcept
    {
        if(this != &other)
        {
            Close();

            socket_ = other.socket_;
            other.socket_ = INVALID_SOCKET;
        }

        return *this;
    }

    bool TcpSocket::Create() {
        Close();

        socket_ = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
        return IsValid();
    }

    bool TcpSocket::Bind(const Ipv4Endpoint& endpoint)
    {
        if(!IsValid()) {
            return false;
        }

        const sockaddr_in& address = endpoint.GetNativeAddress();

        return bind(socket_, reinterpret_cast<const sockaddr*>(&address), sizeof(address)) != SOCKET_ERROR;
    }

    bool TcpSocket::Listen(int backlog) {
        return IsValid() && listen(socket_, backlog) != SOCKET_ERROR;
    }

    bool TcpSocket::Connect(const Ipv4Endpoint& endpoint) {
        if(!IsValid()) {
            return false;
        }

        const sockaddr_in& address = endpoint.GetNativeAddress();

        return connect(socket_, reinterpret_cast<const sockaddr*>(&address), sizeof(address)) != SOCKET_ERROR;
    }

    TcpSocket TcpSocket::Accept(Ipv4Endpoint* remoteEndpoint) {
        sockaddr_in remoteAddress{};
        int remoteAddressSize = sizeof(remoteAddress);

        const SOCKET acceptedSocket = accept(socket_, reinterpret_cast<sockaddr*>(&remoteAddress), &remoteAddressSize);

        if(acceptedSocket == INVALID_SOCKET) {
            return TcpSocket();
        }

        if(remoteEndpoint != nullptr) {
            remoteEndpoint->GetNativeAddress() = remoteAddress;
        }

        return TcpSocket(acceptedSocket);
    }

    bool TcpSocket::SendAll(const void* data, size_t size)
    {
        if(!IsValid() ||
            data == nullptr ||
            size == 0 ||
            size > static_cast<size_t>((std::numeric_limits<int>::max)()))
        {
            return false;
        }

        const char* current = static_cast<const char*>(data);
        int remaining = static_cast<int>(size);

        while(remaining > 0)
        {
            const int sent = send(socket_, current, remaining, 0);

            if(sent == SOCKET_ERROR || sent == 0) {
                return false;
            }

            current += sent;
            remaining -= sent;
        }

        return true;
    }

    bool TcpSocket::ReceiveAll(void* data, size_t size)
    {
        if(!IsValid() ||
            data == nullptr ||
            size == 0 ||
            size > static_cast<size_t>((std::numeric_limits<int>::max)()))
        {
            return false;
        }

        char* current = static_cast<char*>(data);
        int remaining = static_cast<int>(size);

        while(remaining > 0)
        {
            const int received = recv(socket_, current, remaining, 0);

            if(received == SOCKET_ERROR || received == 0) {
                return false;
            }

            current += received;
            remaining -= received;
        }

        return true;
    }

    int TcpSocket::Receive(void* buffer, int bufferSize)
    {
        if(!IsValid() || buffer == nullptr || bufferSize <= 0)
        {
            return SOCKET_ERROR;
        }

        return recv(socket_, static_cast<char*>(buffer), bufferSize, 0);
    }

    bool TcpSocket::IsValid() const {
        return socket_ != INVALID_SOCKET;
    }

    SOCKET TcpSocket::GetNativeSocket() const {
        return socket_;
    }

    void TcpSocket::Close() {
        if(IsValid())
        {
            closesocket(socket_);
            socket_ = INVALID_SOCKET;
        }
    }

    int TcpSocket::GetLastError() {
        return WSAGetLastError();
    }

} // namespace Common