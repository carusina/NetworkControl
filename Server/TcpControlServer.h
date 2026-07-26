#pragma once

#include "../Common/TcpSocket.h"
#include "SessionManager.h"

#include <atomic>
#include <cstdint>
#include <mutex>
#include <thread>
#include <vector>

namespace Server {

	// TCP 연결 수락과 세션별 제어 명령 처리를 담당
	class TcpControlServer {
		public:
			explicit TcpControlServer(SessionManager& sessionManager);
			~TcpControlServer();

			TcpControlServer(const TcpControlServer&) = delete;
			TcpControlServer& operator=(const TcpControlServer&) = delete;

			bool Start(uint16_t port);
			void Stop();

		private:
			void AcceptWorker();
			void ControlWorker(std::shared_ptr<ClientSession> session, Common::Ipv4Endpoint clientEndpoint);

			bool RegisterUdpEndpoint(ClientSession& session, const Common::Ipv4Endpoint& clientEndpoint, uint32_t udpPort) const;

		private:
			SessionManager& sessionManager_;
			Common::TcpSocket listenerSocket_;

			std::thread acceptThread_;
			std::vector<std::thread> controlThreads_;
			std::mutex controlThreadsMutex_;

			std::atomic<bool> isRunning_{ false };
			std::atomic<uint64_t> nextSessionId_{ 1 };
	};

} // namespace Server