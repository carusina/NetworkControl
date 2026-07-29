#include "UdpControlInputReceiver.h"

#include "../Common/BinarySerializer.h"
#include "../Common/Ipv4Endpoint.h"
#include "../Common/Protocol.h"

#include <iostream>

namespace Server {

	UdpControlInputReceiver::UdpControlInputReceiver(SessionManager& sessionManager)
		: sessionManager_(sessionManager) { }

	UdpControlInputReceiver::~UdpControlInputReceiver() {
		Stop();
	}

	bool UdpControlInputReceiver::Start(uint16_t port)
	{
		if (!udpSocket_.Create() ||
			!udpSocket_.Bind(Common::Ipv4Endpoint::Any(port)) ||
			!udpSocket_.SetReceiveTimeout(100))
		{
			std::cerr << "UDP control input receiver start failed: " << Common::UdpSocket::GetLastError() << std::endl;
			udpSocket_.Close();
			return false;
		}

		isRunning_ = true;
		workerThread_ = std::thread(&UdpControlInputReceiver::ReceiveWorker, this);

		std::cout << "UDP control input receiver is listening on port " << port << "." << std::endl;
		return true;
	}

	void UdpControlInputReceiver::Stop()
	{
		isRunning_ = false;

		if (workerThread_.joinable()) {
			workerThread_.join();
		}

		udpSocket_.Close();
	}

	void UdpControlInputReceiver::ReceiveWorker()
	{
		while (isRunning_)
		{
			uint8_t buffer[64]{};
			Common::Ipv4Endpoint sender;
			const int received = udpSocket_.ReceiveFrom(buffer, sizeof(buffer), sender);

			if (received <= 0) {
				continue;
			}

			Common::BinaryReader reader(buffer, static_cast<size_t>(received));

			Common::MessageType type{};
			if (!Common::TryDeserializeHeader(reader, type) || type != Common::MessageType::EntityControlInput) {
				continue;
			}

			Common::EntityControlInputPayload payload;
			if (!Common::TryDeserializeEntityControlInput(reader, payload)) {
				continue;
			}

			// EntityId(= SessionId)로 바로 조회 - 세션 전체를 순회하지 않음
			const auto session = sessionManager_.GetSession(payload.EntityId);
			if (!session) {
				continue;
			}

			// 발신 주소가 그 세션이 등록해둔 UDP 엔드포인트와 일치하는지 확인 (다른 세션 사칭 방지)
			std::string registeredIp;
			uint16_t registeredPort = 0;

			if (!session->TryGetUdpEndpoint(registeredIp, registeredPort) ||
				registeredIp != sender.GetIpAddress() ||
				registeredPort != sender.GetPort())
			{
				continue;
			}

			session->ApplyControlInput(payload.SequenceId, payload.Throttle, payload.Yaw);
		}
	}

} // namespace Server
