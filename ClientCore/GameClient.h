#pragma once

#include "EntityWorld.h"
#include "MetricsCollector.h"
#include "TcpMessageReceiver.h"
#include "UdpReceiver.h"

#include "../Common/Config.h"
#include "../Common/Ipv4Endpoint.h"
#include "../Common/Protocol.h"
#include "../Common/TcpSocket.h"

#include <cstdint>
#include <memory>
#include <vector>

namespace Common { class BinaryWriter; }

namespace Client {

	// 콘솔/GUI 어느 쪽에서도 그대로 쓸 수 있는 클라이언트 네트워킹 파사드.
	// TCP 접속+핸드셰이크, 명령 전송, UDP 송수신, 엔티티/통계 상태 관리를 전부 캡슐화.
	// 콘솔 I/O(std::cin/cout, conio.h 등)는 여기 없음 - REPL이든 GUI든 호출부가 알아서 표시.
	class GameClient {
		public:
			GameClient();
			~GameClient() = default;

			GameClient(const GameClient&) = delete;
			GameClient& operator=(const GameClient&) = delete;

			// TCP 접속 + RegisterUdpPort 핸드셰이크까지 성공해야 true
			bool Connect(const Common::ClientConfig& config);
			void Disconnect();

			// 실패하면 false (TCP 전송 실패 = 연결이 끊겼다고 보고 호출부가 처리)
			bool Play();
			bool Pause();
			bool Stop();
			bool Reset();
			bool SetRate(Common::DataRate dataRate);

			// throttle/yaw는 내부에서 -1..1로 클램프됨. 아직 내 EntityId를 모르면(Spawn 수신 전) false
			bool SendControlInput(float throttle, float yaw);

			bool TryGetMyEntityId(uint32_t& entityId) const;
			std::vector<EntityInfo> GetEntities() const;
			MetricsSnapshot GetMetrics() const;

		private:
			bool SendMessage(const Common::BinaryWriter& writer);
			bool SendHeaderOnly(Common::MessageType type);
			bool SendRegisterUdpPort(uint16_t port);
			bool SendSetRate(uint32_t dataRateHz);

		private:
			EntityWorld entityWorld_;
			UdpReceiver udpReceiver_;
			Common::TcpSocket tcpSocket_;
			Common::Ipv4Endpoint serverUdpEndpoint_;
			std::unique_ptr<TcpMessageReceiver> tcpMessageReceiver_;
			bool isConnected_ = false;
	};

} // namespace Client
