#pragma once

#include "EntityWorld.h"
#include "MetricsCollector.h"

#include "../Common/Ipv4Endpoint.h"
#include "../Common/Protocol.h"
#include "../Common/UdpSocket.h"

#include <atomic>
#include <cstdint>
#include <thread>

namespace Client {

	// EntityState UDP 패킷을 수신해 EntityWorld/MetricsCollector에 반영하고, 조종 입력을 서버로 송신
	class UdpReceiver {
		public:
			explicit UdpReceiver(EntityWorld& entityWorld);
			~UdpReceiver();

			UdpReceiver(const UdpReceiver&) = delete;
			UdpReceiver& operator=(const UdpReceiver&) = delete;

			bool Start(uint16_t port);
			void Stop();

			void ResetMetrics();
			MetricsSnapshot GetMetrics() const;

			// 현재 서버 전송률을 수신 간격 측정 기준으로 사용
			void SetExpectedDataRate(Common::DataRate dataRate);

			// Pause, Stop, Play 이후 오래된 수신 시각을 기준으로 삼지 않음
			void ResetReceiveTiming();

			// 조종 입력을 서버로 전송 - Start()에서 바인드해둔 소켓을 그대로 사용
			bool SendControlInput(const Common::Ipv4Endpoint& serverEndpoint, float throttle, float yaw);

		private:
			void ReceiveWorker();

		private:
			Common::UdpSocket udpSocket_;
			EntityWorld& entityWorld_;
			MetricsCollector metricsCollector_;
			std::thread workerThread_;
			std::atomic<bool> isRunning_{ false };
	};

} // namespace Client
