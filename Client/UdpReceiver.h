#pragma once

#include "MetricsCollector.h"

#include "../Common/UdpSocket.h"
#include "../Common/Protocol.h"

#include <atomic>
#include <cstdint>
#include <thread>

namespace Client {

	// UDP Packet을 수신하고 MetricsCollector에 전달
	class UdpReceiver {
		public:
			UdpReceiver() = default;
			~UdpReceiver();

			UdpReceiver(const UdpReceiver&) = delete;
			UdpReceiver& operator=(const UdpReceiver&) = delete;

			bool Start(uint16_t port);
			void Stop();

			void ResetMetrics();
			MetricsSnapshot GetMetrics() const;

			// 현재 서버 전송률을 수신 간격 측정 기준으로 사용
			void SetExpectedDataRate(Common::DataRate dataRate);

			// Pause, Stop, Play 이후 오래도니 수신 시각을 제외
			void ResetReceiveTiming();

		private:
			void ReceiveWorker();

		private:
			Common::UdpSocket udpSocket_;
			MetricsCollector metricsCollector_;
			std::thread workerThread_;
			std::atomic<bool> isRunning_{ false };
	};

} // namespace Client