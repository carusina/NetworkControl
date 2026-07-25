#include "UdpSocket.h"

namespace Common {

    UdpSocket::UdpSocket(SOCKET socket) :
        socket_(socket) {}

    UdpSocket::~UdpSocket() {
        Close();
    }

    UdpSocket::UdpSocket(UdpSocket&& other) noexcept
        : socket_(other.socket_)
    {
        other.socket_ = INVALID_SOCKET;
    }

    UdpSocket& UdpSocket::operator=(UdpSocket&& other) noexcept {
        if(this != &other) {
            Close();

            socket_ = other.socket_;
            other.socket_ = INVALID_SOCKET;
        }

        return *this;
    }

    bool UdpSocket::Create() {
        Close();

        socket_ = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
        return IsValid();
    }

    bool UdpSocket::Bind(const Ipv4Endpoint& endpoint) {
        if(!IsValid()) {
            return false;
        }

        const sockaddr_in& address = endpoint.GetNativeAddress();

        return bind(socket_, reinterpret_cast<const sockaddr*>(&address), sizeof(address)) != SOCKET_ERROR;
    }

    int UdpSocket::SendTo(const void* data, int size, const Ipv4Endpoint& destination)
    {
        if(!IsValid() || data == nullptr || size <= 0)
        {
            return SOCKET_ERROR;
        }

        const sockaddr_in& address = destination.GetNativeAddress();

        return sendto(socket_, static_cast<const char*>(data), size, 0, reinterpret_cast<const sockaddr*>(&address), sizeof(address));
    }

    int UdpSocket::ReceiveFrom(void* buffer, int bufferSize, Ipv4Endpoint& sender)
    {
        if(!IsValid() || buffer == nullptr || bufferSize <= 0)
        {
            return SOCKET_ERROR;
        }

        sockaddr_in senderAddress{};
        int senderAddressSize = sizeof(senderAddress);

        const int received = recvfrom(socket_, static_cast<char*>(buffer), bufferSize, 0, reinterpret_cast<sockaddr*>(&senderAddress), &senderAddressSize);

        if(received != SOCKET_ERROR) {
            sender.GetNativeAddress() = senderAddress;
        }

        return received;
    }

    bool UdpSocket::IsValid() const {
        return socket_ != INVALID_SOCKET;
    }

    SOCKET UdpSocket::GetNativeSocket() const {
        return socket_;
    }

    void UdpSocket::Close() {
        if(IsValid()) {
            closesocket(socket_);
            socket_ = INVALID_SOCKET;
        }
    }

    int UdpSocket::GetLastError() {
        return WSAGetLastError();
    }

    bool UdpSocket::SetReceiveTimeout(uint32_t timeoutMilliseconds) {
        if(!IsValid()) {
            return false;
        }

        const DWORD timeout = static_cast<DWORD>(timeoutMilliseconds);

        return setsockopt(socket_, SOL_SOCKET, SO_RCVTIMEO, reinterpret_cast<const char*>(&timeout), sizeof(timeout)) != SOCKET_ERROR;
    }

} // namespace Common