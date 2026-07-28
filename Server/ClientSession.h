#pragma once

#include "../Common/Protocol.h"
#include "../Common/TcpSocket.h"

#include <atomic>
#include <cstdint>
#include <memory>
#include <mutex>
#include <string>

namespace Server {

	// 연결된 클라이언트 한 명의 제어 연결과 엔티티(헬기) 시뮬레이션 상태를 보관
	class ClientSession {
		public:
			ClientSession(uint64_t sessionId, std::shared_ptr<Common::TcpSocket> controlSocket);
			~ClientSession() = default;

			void Play();
			void Pause();
			void Stop();
			void Reset();
			void SetRate(uint32_t dataRateHz);

			void RegisterUdpEndpoint(const std::string& ip, uint16_t port);

			uint64_t GetSessionId() const;
			uint32_t GetEntityId() const;
			Common::SessionState GetState() const;
			Common::DataRate GetDataRate() const;
			uint64_t GetNextSequenceId();

			bool TryGetUdpEndpoint(std::string& ip, uint16_t& port) const;

			Common::TcpSocket& GetControlSocket();

			// 조종 입력 반영 - UdpControlInputReceiver 스레드가 호출
			void ApplyControlInput(float throttle, float yaw);

			// 매 틱 물리 갱신 - UdpStreamingService 워커 스레드가 호출
			void StepPhysics(double deltaSeconds);

			Common::EntityStatePayload BuildEntityStatePayload(uint64_t sequenceId, uint64_t timestampMicroseconds) const;
			Common::EntitySpawnPayload BuildEntitySpawnPayload() const;

			// 서버 종료 또는 연결 해제 시 세션을 정리
			void StopSession();

		private:
			uint64_t sessionId_;
			std::shared_ptr<Common::TcpSocket> controlSocket_;

			std::atomic<Common::SessionState> state_{ Common::SessionState::Stopped };
			std::atomic<Common::DataRate> dataRate_{ Common::DataRate::Hz30 };
			std::atomic<uint64_t> nextSequenceId_{ 0 };
			std::atomic<bool> isControlConnectionClosed_{ false };

			mutable std::mutex endpointMutex_;
			std::string udpIpAddress_;
			uint16_t udpPort_ = 0;

			// 엔티티(헬기) 시뮬레이션 상태 - 물리 틱 스레드와 조종 입력 스레드가 동시에 접근
			mutable std::mutex entityMutex_;
			float positionX_ = 0.0f;
			float positionY_ = 0.0f;
			float heading_ = 0.0f;
			float speed_ = 0.0f;
			float velocityX_ = 0.0f;
			float velocityY_ = 0.0f;
			float throttle_ = 0.0f;
			float yaw_ = 0.0f;
	};

} // namespace Server
