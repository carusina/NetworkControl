#pragma once

#include "../Common/UdpSocket.h"
#include "SessionManager.h"

#include <atomic>
#include <thread>

namespace Server {

	// Playing 상태 세션에 UDP Packet을 주기적으로 전송
	class UdpStreamingService {
		public:
			explicit UdpStreamingService(SessionManager& sessionManager);
			~UdpStreamingService();

			UdpStreamingService(const UdpStreamingService&) = delete;
			UdpStreamingService& operator=(const UdpStreamingService&) = delete;

			bool Start();
			void Stop();

		private:
			void Worker();

		private:
			SessionManager& sessionManager_;
			Common::UdpSocket udpSocket_;
			std::thread workerThread_;
			std::atomic<bool> isRunning_{ false };
	};

} // namespace Server