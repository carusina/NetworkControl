#pragma once

namespace Common {

    // WinSock 초기화와 정리를 관리
    class SocketRuntime {
        public:
            SocketRuntime();
            ~SocketRuntime();

            SocketRuntime(const SocketRuntime&) = delete;
            SocketRuntime& operator=(const SocketRuntime&) = delete;

            bool IsInitialized() const;

        private:
            bool isInitialized_ = false;
    };

} // namespace Common