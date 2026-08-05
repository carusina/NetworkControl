#pragma once

#include "EntityWorld.h"
#include "MetricsCollector.h"
#include "TcpMessageReceiver.h"
#include "UdpReceiver.h"

#include "../Common/Config.h"
#include "../Common/Ipv4Endpoint.h"
#include "../Common/Protocol.h"
#include "../Common/TcpSocket.h"

#include <atomic>
#include <cstdint>
#include <memory>
#include <mutex>
#include <thread>
#include <vector>

namespace Common { class BinaryWriter; }

namespace Client {

	// 서버와의 현재 연결 상태 - GetConnectionState()로 폴링해서 확인
	enum class ConnectionState {
		Disconnected,   // 연결된 적 없거나, Disconnect()를 명시적으로 호출함
		Connected,
		Reconnecting    // 접속 중 예기치 않게 끊겨서 백그라운드에서 재접속을 시도하는 중
	};

	// 콘솔/GUI 어느 쪽에서도 그대로 쓸 수 있는 클라이언트 네트워킹 파사드.
	// TCP 접속+핸드셰이크, 명령 전송, UDP 송수신, 엔티티/통계 상태 관리를 전부 캡슐화.
	// 콘솔 I/O(std::cin/cout, conio.h 등)는 여기 없음 - REPL이든 GUI든 호출부가 알아서 표시.
	class GameClient {
		public:
			GameClient();
			~GameClient();

			GameClient(const GameClient&) = delete;
			GameClient& operator=(const GameClient&) = delete;

			// TCP 접속 + RegisterUdpPort 핸드셰이크까지 성공해야 true. 성공하면 백그라운드
			// 감시 스레드가 시작되어, 이후 연결이 예기치 않게 끊기면 자동으로 재접속을 시도한다
			bool Connect(const Common::ClientConfig& config);

			// 사용자가 명시적으로 끊는 경우 - 자동 재접속 감시도 같이 멈춘다
			void Disconnect();

			bool IsConnected() const;
			ConnectionState GetConnectionState() const;

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
			// connectionMutex_를 이미 잡은 상태에서 호출 - 실제 접속 시퀀스(수동 Connect()와
			// 워치독의 재접속 시도가 공유)
			bool ConnectLocked(const Common::ClientConfig& config);

			// connectionMutex_를 이미 잡은 상태에서 호출 - 소켓/스레드 정리
			void TeardownConnectionLocked();

			// 연결이 예기치 않게 끊겼는지 주기적으로 확인하고, 끊겼으면 재접속을 시도하는 백그라운드 스레드
			void WatchdogWorker();

			bool SendMessage(const Common::BinaryWriter& writer);
			bool SendHeaderOnly(Common::MessageType type);
			bool SendRegisterUdpPort(uint16_t port);
			bool SendSetRate(uint32_t dataRateHz);
			bool SendStop(const MetricsSnapshot& metrics);

		private:
			EntityWorld entityWorld_;
			UdpReceiver udpReceiver_;
			Common::TcpSocket tcpSocket_;
			Common::Ipv4Endpoint serverUdpEndpoint_;
			std::unique_ptr<TcpMessageReceiver> tcpMessageReceiver_;

			// Connect()/Disconnect()/WatchdogWorker()가 위 소켓/스레드 멤버를 동시에 건드리지
			// 않도록 보호 (Play/Pause 등 명령 송신 경로는 여기 안 걸림 - GameClient::Connect
			// 위쪽 주석 및 README "동시성 설계 메모" 참고)
			std::mutex connectionMutex_;
			std::atomic<ConnectionState> connectionState_{ ConnectionState::Disconnected };
			Common::ClientConfig lastConfig_;

			std::thread watchdogThread_;
			std::atomic<bool> isWatchdogRunning_{ false };
	};

} // namespace Client
