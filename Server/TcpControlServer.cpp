#include "TcpControlServer.h"

#include "../Common/BinarySerializer.h"

#include <chrono>
#include <iomanip>
#include <iostream>
#include <memory>
#include <thread>
#include <utility>

namespace Server {

	TcpControlServer::TcpControlServer(SessionManager& sessionManager)
		: sessionManager_(sessionManager) { }

	TcpControlServer::~TcpControlServer() {
		Stop();
	}

	bool TcpControlServer::Start(uint16_t port)
	{
		if (isRunning_) {
			return false;
		}

		if (!listenerSocket_.Create() ||
			!listenerSocket_.Bind(Common::Ipv4Endpoint::Any(port)) ||
			!listenerSocket_.Listen(SOMAXCONN))
		{
			std::cerr << "TCP listener start failed: " << Common::TcpSocket::GetLastError() << std::endl;
			return false;
		}

		isRunning_ = true;
		acceptThread_ = std::thread(&TcpControlServer::AcceptWorker, this);

		std::cout << "TCP control server is listening on port " << port << "." << std::endl;
		return true;
	}

	void TcpControlServer::Stop()
	{
		if (!isRunning_.exchange(false)) {
			return;
		}

		// accept 대기를 해제
		listenerSocket_.Close();

		// 각 제어 스레드의 ReceiveAll 대기를 해제
		const auto sessions = sessionManager_.GetSessionsSnapshot();
		for (const auto& session : sessions) {
			session->StopSession();
		}

		if (acceptThread_.joinable()) {
			acceptThread_.join();
		}

		// acceptThread_가 끝났으므로 이 시점 이후로는 activeControlWorkerCount_가 늘어나지 않음
		while (activeControlWorkerCount_.load() > 0) {
			std::this_thread::sleep_for(std::chrono::milliseconds(10));
		}
	}

	void TcpControlServer::AcceptWorker()
	{
		while (isRunning_)
		{
			Common::Ipv4Endpoint clientEndpoint;
			Common::TcpSocket acceptedSocket = listenerSocket_.Accept(&clientEndpoint);

			if (!acceptedSocket.IsValid())
			{
				if (isRunning_) {
					std::cerr << "TCP accept failed: " << Common::TcpSocket::GetLastError() << std::endl;
				}

				continue;
			}

			if (!isRunning_) {
				acceptedSocket.Close();
				break;
			}

			const auto controlSocket = std::make_shared<Common::TcpSocket>(std::move(acceptedSocket));
			const auto session = std::make_shared<ClientSession>(nextSessionId_.fetch_add(1), controlSocket);

			sessionManager_.AddSession(session);

			std::cout << "Session " << session->GetSessionId() << " connection from " << clientEndpoint.GetIpAddress() << "." << std::endl;

			activeControlWorkerCount_.fetch_add(1);
			std::thread(&TcpControlServer::ControlWorker, this, session, clientEndpoint).detach();
		}
	}

	void TcpControlServer::ControlWorker(std::shared_ptr<ClientSession> session, Common::Ipv4Endpoint clientEndpoint)
	{
		bool isUdpEndpointRegistered = false;

		while (isRunning_ && ProcessOneMessage(*session, clientEndpoint, isUdpEndpointRegistered)) { }

		std::cout << "Session " << session->GetSessionId() << " control connection closed." << std::endl;

		// 한 번도 등록 안 된 세션은 다른 클라이언트에게 존재를 알린 적이 없으므로 Despawn도 필요 없음
		if (isUdpEndpointRegistered) {
			BroadcastEntityDespawn(session->GetEntityId(), session->GetSessionId());
		}

		sessionManager_.RemoveSession(session->GetSessionId());
		activeControlWorkerCount_.fetch_sub(1);
	}

	bool TcpControlServer::ProcessOneMessage(ClientSession& session, const Common::Ipv4Endpoint& clientEndpoint, bool& isUdpEndpointRegistered)
	{
		Common::TcpSocket& controlSocket = session.GetControlSocket();

		uint8_t headerByte = 0;
		if (!controlSocket.ReceiveAll(&headerByte, sizeof(headerByte))) {
			return false;
		}

		Common::MessageType type{};
		{
			Common::BinaryReader headerReader(&headerByte, sizeof(headerByte));
			Common::TryDeserializeHeader(headerReader, type);
		}

		if (type == Common::MessageType::RegisterUdpPort)
		{
			uint8_t payloadBytes[Common::RegisterUdpPortPayloadSize]{};
			if (!controlSocket.ReceiveAll(payloadBytes, sizeof(payloadBytes))) {
				return false;
			}

			Common::RegisterUdpPortPayload payload{};
			Common::BinaryReader reader(payloadBytes, sizeof(payloadBytes));
			if (!Common::TryDeserializeRegisterUdpPort(reader, payload)) {
				return false;
			}

			isUdpEndpointRegistered = RegisterUdpEndpoint(session, clientEndpoint, payload.Port);

			if (isUdpEndpointRegistered) {
				BroadcastSpawnsForNewSession(session);
			}

			return true;
		}

		if (type == Common::MessageType::SetRate)
		{
			uint8_t payloadBytes[Common::SetRatePayloadSize]{};
			if (!controlSocket.ReceiveAll(payloadBytes, sizeof(payloadBytes))) {
				return false;
			}

			if (!isUdpEndpointRegistered) {
				std::cout << "[Session " << session.GetSessionId() << "] Command ignored: UDP port is not registered." << std::endl;
				return true;
			}

			Common::SetRatePayload payload{};
			Common::BinaryReader reader(payloadBytes, sizeof(payloadBytes));
			if (!Common::TryDeserializeSetRate(reader, payload)) {
				return false;
			}

			session.SetRate(payload.DataRateHz);
			return true;
		}

		if (type == Common::MessageType::Stop)
		{
			uint8_t payloadBytes[Common::StopPayloadSize]{};
			if (!controlSocket.ReceiveAll(payloadBytes, sizeof(payloadBytes))) {
				return false;
			}

			if (!isUdpEndpointRegistered) {
				std::cout << "[Session " << session.GetSessionId() << "] Command ignored: UDP port is not registered." << std::endl;
				return true;
			}

			Common::StopPayload payload{};
			Common::BinaryReader reader(payloadBytes, sizeof(payloadBytes));
			if (!Common::TryDeserializeStop(reader, payload)) {
				return false;
			}

			// session.Stop()이 nextSequenceId_를 0으로 되돌리므로, GetSentPacketCount()는 그 전에 출력
			PrintClientMetricsSnapshot(session, payload);
			session.Stop();
			return true;
		}

		// 여기부터는 Payload 없는 명령(Play/Pause/Reset)
		if (!isUdpEndpointRegistered) {
			std::cout << "[Session " << session.GetSessionId() << "] Command ignored: UDP port is not registered." << std::endl;
			return true;
		}

		switch (type)
		{
			case Common::MessageType::Play:
				session.Play();
				return true;

			case Common::MessageType::Pause:
				session.Pause();
				return true;

			case Common::MessageType::Reset:
				session.Reset();
				return true;

			default:
				std::cerr << "[Session " << session.GetSessionId() << "] Unexpected message type on control channel: "
					<< static_cast<int>(type) << std::endl;
				return false;
		}
	}

	bool TcpControlServer::RegisterUdpEndpoint(ClientSession& session, const Common::Ipv4Endpoint& clientEndpoint, uint16_t udpPort) const
	{
		if (udpPort == 0) {
			std::cerr << "Invalid UDP port: 0" << std::endl;
			return false;
		}

		session.RegisterUdpEndpoint(clientEndpoint.GetIpAddress(), udpPort);
		return true;
	}

	void TcpControlServer::PrintClientMetricsSnapshot(const ClientSession& session, const Common::StopPayload& metrics) const
	{
		std::cout << std::fixed << std::setprecision(2);

		std::cout << "[Session " << session.GetSessionId() << "] Stop - client MetricsSnapshot:" << std::endl;
		std::cout << "  Elapsed: " << metrics.ElapsedSeconds << "s" << std::endl;
		std::cout << "  Sent to this client: " << session.GetSentPacketCount() << " packets" << std::endl;
		std::cout << "  Received: " << metrics.TotalReceivedCount << ", Loss: " << metrics.LossCount
			<< ", Loss Rate: " << metrics.LossRate << "%, Out of Order: " << metrics.OutOfOrderCount << std::endl;
		std::cout << "  Average Interval: " << metrics.AverageReceiveIntervalMilliseconds << "ms, Max Interval: "
			<< metrics.MaxReceiveIntervalMilliseconds << "ms, Average Deviation: " << metrics.AverageIntervalDeviationMilliseconds << "ms" << std::endl;
		std::cout << "  Delayed Packets: " << metrics.DelayedPacketCount << " / " << metrics.IntervalSampleCount
			<< ", Delayed Packet Rate: " << metrics.DelayedPacketRate << "%" << std::endl;
		std::cout << "  Latency: avg " << metrics.AverageLatencyMilliseconds << "ms, min " << metrics.MinLatencyMilliseconds
			<< "ms, max " << metrics.MaxLatencyMilliseconds << "ms (" << metrics.LatencySampleCount << " samples)" << std::endl;
	}

	void TcpControlServer::SendEntitySpawn(ClientSession& recipient, const Common::EntitySpawnPayload& payload) const
	{
		Common::BinaryWriter writer;
		Common::SerializeHeader(writer, Common::MessageType::EntitySpawn);
		Common::SerializeEntitySpawn(writer, payload);

		const auto& data = writer.Data();
		if (!recipient.GetControlSocket().SendAll(data.data(), data.size())) {
			std::cerr << "[Session " << recipient.GetSessionId() << "] Failed to send EntitySpawn." << std::endl;
		}
	}

	void TcpControlServer::SendEntityDespawn(ClientSession& recipient, const Common::EntityDespawnPayload& payload) const
	{
		Common::BinaryWriter writer;
		Common::SerializeHeader(writer, Common::MessageType::EntityDespawn);
		Common::SerializeEntityDespawn(writer, payload);

		const auto& data = writer.Data();
		if (!recipient.GetControlSocket().SendAll(data.data(), data.size())) {
			std::cerr << "[Session " << recipient.GetSessionId() << "] Failed to send EntityDespawn." << std::endl;
		}
	}

	void TcpControlServer::BroadcastSpawnsForNewSession(ClientSession& newSession)
	{
		const auto sessions = sessionManager_.GetSessionsSnapshot();

		// 1) 새 클라이언트에게 자기 자신의 Spawn을 가장 먼저 보냄 (첫 Spawn = 내 EntityId 규약)
		SendEntitySpawn(newSession, newSession.BuildEntitySpawnPayload());

		// 2) 이미 있던 다른 세션들의 Spawn을 새 클라이언트에게 전달 (늦게 접속해도 기존 참가자를 볼 수 있게)
		for (const auto& other : sessions)
		{
			if (other->GetSessionId() != newSession.GetSessionId()) {
				SendEntitySpawn(newSession, other->BuildEntitySpawnPayload());
			}
		}

		// 3) 새 세션의 Spawn을 다른 모든 기존 세션에 브로드캐스트
		for (const auto& other : sessions)
		{
			if (other->GetSessionId() != newSession.GetSessionId()) {
				SendEntitySpawn(*other, newSession.BuildEntitySpawnPayload());
			}
		}
	}

	void TcpControlServer::BroadcastEntityDespawn(uint32_t entityId, uint64_t excludeSessionId)
	{
		Common::EntityDespawnPayload payload;
		payload.EntityId = entityId;

		for (const auto& other : sessionManager_.GetSessionsSnapshot())
		{
			if (other->GetSessionId() != excludeSessionId) {
				SendEntityDespawn(*other, payload);
			}
		}
	}

} // namespace Server
