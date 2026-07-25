#include "SocketRuntime.h"

#include <WinSock2.h>
#include <WS2tcpip.h>

#pragma comment(lib, "Ws2_32.lib")

namespace Common {

    SocketRuntime::SocketRuntime() {
        WSADATA wsaData{};
        isInitialized_ = WSAStartup(MAKEWORD(2, 2), &wsaData) == 0;
    }

    SocketRuntime::~SocketRuntime() {
        if(isInitialized_) {
            WSACleanup();
        }
    }

    bool SocketRuntime::IsInitialized() const {
        return isInitialized_;
    }

} // namespace Common