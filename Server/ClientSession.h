#pragma once

#include "../Common/Protocol.h"
#include "../Common/TcpSocket.h"

#include <atomic>
#include <cstdint>
#include <memory>
#include <mutex>
#include <string>

namespace Server {

	// 연결된 클라이언트 한 명의 제어 연결과 UDP 송신 상태를 보관
	class ClientSession {
		public:
			ClientSession(uint64_t sessionId, std::shared_ptr<Common::TcpSocket> controlSocket);
			~ClientSession() = default;

			void ProcessCommand(Common::CommandType command, uint32_t payload);
			void RegisterUdpEndpoint(const std::string& ip, uint16_t port);

			uint64_t GetSessionId() const;
			Common::SessionState GetState() const;
			Common::DataRate GetDataRate() const;
			uint64_t GetNextSequenceId();

			bool TryGetUdpEndpoint(std::string& ip, uint16_t& port) const;
		
			Common::TcpSocket& GetControlSocket();

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
	};

} // namespace Server