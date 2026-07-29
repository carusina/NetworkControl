#pragma once

#include "../Common/UdpSocket.h"
#include "SessionManager.h"

#include <atomic>
#include <memory>
#include <thread>

namespace Server {

	// 클라이언트가 UDP로 보내는 EntityControlInput을 받아 해당 세션에 반영
	class UdpControlInputReceiver {
		public:
			explicit UdpControlInputReceiver(SessionManager& sessionManager);
			~UdpControlInputReceiver();

			UdpControlInputReceiver(const UdpControlInputReceiver&) = delete;
			UdpControlInputReceiver& operator=(const UdpControlInputReceiver&) = delete;

			bool Start(uint16_t port);
			void Stop();

		private:
			void ReceiveWorker();

		private:
			SessionManager& sessionManager_;
			Common::UdpSocket udpSocket_;
			std::thread workerThread_;
			std::atomic<bool> isRunning_{ false };
	};

} // namespace Server
