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

			// Play/Pause/Stop 전환 시 호출. MetricsCollector::OnPlay/OnPause로 전달하는 것 외에도
			// isActive_를 켜고 끔 - 꺼져 있는 동안 도착하는 EntityState는 ReceiveWorker가 그냥 버림.
			// 이게 없으면, 로컬에서 이미 Pause/Stop을 반영한 뒤에도 서버가 그 명령을 처리하기 전에
			// 이미 날아오고 있던 패킷이 뒤늦게 도착해서, 방금 리셋한 통계를 다시 "첫 패킷"으로 오인하고
			// 경과 시간이 다시 흐르거나 SequenceId 기준선이 어긋나는 문제가 생김
			void OnPlay();
			void OnPause();
			void OnStop();

			// 조종 입력을 서버로 전송 - Start()에서 바인드해둔 소켓을 그대로 사용
			bool SendControlInput(const Common::Ipv4Endpoint& serverEndpoint, uint32_t entityId, float throttle, float yaw);

		private:
			void ReceiveWorker();

		private:
			Common::UdpSocket udpSocket_;
			EntityWorld& entityWorld_;
			MetricsCollector metricsCollector_;
			std::thread workerThread_;
			std::atomic<bool> isRunning_{ false };

			// 로컬에서 재생 중이라고 믿는 동안만 true - Connect() 직후나 Pause/Stop 이후엔 false
			std::atomic<bool> isActive_{ false };

			// 조종 입력을 보낼 때마다 증가 - 서버가 UDP 역전을 걸러내는 데 사용
			std::atomic<uint32_t> nextControlInputSequenceId_{ 0 };
	};

} // namespace Client
