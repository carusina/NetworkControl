#include "UdpStreamingService.h"

#include "../Common/HighResolutionTimer.h"
#include "../Common/Ipv4Endpoint.h"
#include "../Common/Protocol.h"
#include "../Common/TimerCompensator.h"

#include <iostream>
#include <string>

namespace Server {

	UdpStreamingService::UdpStreamingService(SessionManager& sessionManager)
		: sessionManager_(sessionManager) { }

	UdpStreamingService::~UdpStreamingService() {
		Stop();
	}

	bool UdpStreamingService::Start()
	{
		if (isRunning_) {
			return false;
		}

		if (!udpSocket_.Create()) {
			std::cerr << "UDP sender socket creation failed: " << Common::UdpSocket::GetLastError() << std::endl;
			return false;
		}

		isRunning_ = true;
		workerThread_ = std::thread(&UdpStreamingService::Worker, this);

		return true;
	}

	void UdpStreamingService::Stop()
	{
		if (!isRunning_.exchange(false)) {
			return;
		}

		if (workerThread_.joinable()) {
			workerThread_.join();
		}

		udpSocket_.Close();
	}

	void UdpStreamingService::Worker()
	{
		// 60Hz를 기준 틱으로 사용하고, 30Hz는 두 틱마다 전송
		Common::TimerCompensator timer(Common::DataRate::Hz60);
		uint64_t tickCount = 0;

		while (isRunning_)
		{
			timer.WaitForNextTick();
			++tickCount;

			const auto sessions = sessionManager_.GetSessionsSnapshot();

			for (const auto& session : sessions)
			{
				if (session->GetState() != Common::SessionState::Playing) {
					continue;
				}

				if (session->GetDataRate() == Common::DataRate::Hz30 && tickCount % 2 != 0) {
					continue;
				}

				std::string ipAddress;
				uint16_t udpPort = 0;
				
				if (!session->TryGetUdpEndpoint(ipAddress, udpPort)) {
					continue;
				}

				Common::Ipv4Endpoint endpoint;
				if (!Common::Ipv4Endpoint::TryCreate(ipAddress, udpPort, endpoint)) {
					continue;
				}

				Common::Packet packet{};
				packet.SequenceId = session->GetNextSequenceId();
				packet.Timestamp = Common::HighResolutionTimer::GetMicroseconds();

				const int sent = udpSocket_.SendTo(&packet, sizeof(packet), endpoint);

				if (sent != sizeof(packet)) {
					std::cerr << "[Session " << session->GetSessionId() << "] UDP send failed: " << Common::UdpSocket::GetLastError() << std::endl;
				}
			}
		}
	}

} // namespace Server