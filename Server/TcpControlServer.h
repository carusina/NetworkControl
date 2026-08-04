#pragma once

#include "../Common/Protocol.h"
#include "../Common/TcpSocket.h"
#include "SessionManager.h"

#include <atomic>
#include <cstdint>
#include <thread>

namespace Server {

	// TCP 연결 수락과 세션별 제어 명령 처리, 엔티티 Spawn/Despawn 브로드캐스트를 담당
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

			// 메시지 하나를 읽어 처리. 연결을 계속 유지해야 하면 true, 끊어야 하면 false
			bool ProcessOneMessage(ClientSession& session, const Common::Ipv4Endpoint& clientEndpoint, bool& isUdpEndpointRegistered);

			bool RegisterUdpEndpoint(ClientSession& session, const Common::Ipv4Endpoint& clientEndpoint, uint16_t udpPort) const;

			// Stop 처리 직전에 호출 - session.GetSentPacketCount()가 session.Stop()으로 리셋되기 전에
			// 클라이언트가 보낸 마지막 MetricsSnapshot(StopPayload)과 함께 콘솔에 출력
			void PrintClientMetricsSnapshot(const ClientSession& session, const Common::StopPayload& metrics) const;

			void SendEntitySpawn(ClientSession& recipient, const Common::EntitySpawnPayload& payload) const;
			void SendEntityDespawn(ClientSession& recipient, const Common::EntityDespawnPayload& payload) const;

			// 새로 등록된 세션에게: 자기 자신 Spawn(가장 먼저) + 기존 세션들 Spawn / 다른 세션들에게: 새 세션 Spawn
			void BroadcastSpawnsForNewSession(ClientSession& newSession);

			// excludeSessionId를 제외한 모든 세션에 EntityDespawn 전송
			void BroadcastEntityDespawn(uint32_t entityId, uint64_t excludeSessionId);

		private:
			SessionManager& sessionManager_;
			Common::TcpSocket listenerSocket_;

			std::thread acceptThread_;

			// 세션마다 detach된 ControlWorker 스레드 수 - Stop()이 전부 끝나기를 기다리는 데 사용
			std::atomic<int> activeControlWorkerCount_{ 0 };

			std::atomic<bool> isRunning_{ false };
			std::atomic<uint64_t> nextSessionId_{ 1 };
	};

} // namespace Server
