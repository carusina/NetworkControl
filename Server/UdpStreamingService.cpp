#include "UdpStreamingService.h"

#include "../Common/BinarySerializer.h"
#include "../Common/HighResolutionTimer.h"
#include "../Common/Ipv4Endpoint.h"
#include "../Common/Protocol.h"
#include "../Common/TimerCompensator.h"

#include <iostream>
#include <string>
#include <vector>

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
		// 60Hz를 기준 틱으로 사용하고, 30Hz는 두 틱마다 전송. 물리 시뮬레이션은 항상 60Hz로 돌림
		Common::TimerCompensator timer(Common::DataRate::Hz60);
		uint64_t tickCount = 0;

		constexpr double FixedDeltaSeconds = 1.0 / static_cast<double>(static_cast<uint32_t>(Common::DataRate::Hz60));

		while (isRunning_)
		{
			timer.WaitForNextTick();
			++tickCount;

			const auto sessions = sessionManager_.GetSessionsSnapshot();

			// 1) Playing 상태인 모든 엔티티의 물리를 한 틱 진행
			for (const auto& session : sessions)
			{
				if (session->GetState() == Common::SessionState::Playing) {
					session->StepPhysics(FixedDeltaSeconds);
				}
			}

			// 2) 이번 틱에 브로드캐스트할 엔티티 상태를 한 번만 모음 (수신자마다 같은 값이라 다시 안 만듦)
			std::vector<Common::EntityStateEntry> entries;
			for (const auto& session : sessions)
			{
				if (session->GetState() == Common::SessionState::Playing) {
					entries.push_back(session->BuildEntityStateEntry());
				}
			}

			if (entries.empty()) {
				continue;
			}

			const uint64_t timestamp = Common::HighResolutionTimer::GetMicroseconds();

			// 3) Playing 상태인 각 수신자에게 모든 엔티티 상태를 담은 패킷 하나만 전송(수신자당 패킷 1개)
			for (const auto& recipient : sessions)
			{
				if (recipient->GetState() != Common::SessionState::Playing) {
					continue;
				}

				if (recipient->GetDataRate() == Common::DataRate::Hz30 && tickCount % 2 != 0) {
					continue;
				}

				std::string ipAddress;
				uint16_t udpPort = 0;

				if (!recipient->TryGetUdpEndpoint(ipAddress, udpPort)) {
					continue;
				}

				Common::Ipv4Endpoint endpoint;
				if (!Common::Ipv4Endpoint::TryCreate(ipAddress, udpPort, endpoint)) {
					continue;
				}

				Common::EntityStateBatchPayload batch;
				batch.SequenceId = recipient->GetNextSequenceId();
				batch.Timestamp = timestamp;
				batch.Entities = entries;

				Common::BinaryWriter writer;
				Common::SerializeHeader(writer, Common::MessageType::EntityState);
				Common::SerializeEntityStateBatch(writer, batch);

				const auto& data = writer.Data();
				const int sent = udpSocket_.SendTo(data.data(), static_cast<int>(data.size()), endpoint);

				if (sent != static_cast<int>(data.size())) {
					std::cerr << "[Session " << recipient->GetSessionId() << "] UDP send failed: " << Common::UdpSocket::GetLastError() << std::endl;
				}
			}
		}
	}

} // namespace Server