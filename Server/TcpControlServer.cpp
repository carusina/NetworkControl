#include "TcpControlServer.h"
#include "../Common/Protocol.h"

#include <chrono>
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
		Common::ControlMessage message{};

		Common::TcpSocket& controlSocket = session->GetControlSocket();

		while (isRunning_ && controlSocket.ReceiveAll(&message, sizeof(message)))
		{
			message.Payload = ntohl(message.Payload);

			if (message.Type == Common::CommandType::RegisterUdpPort)
			{
				isUdpEndpointRegistered = RegisterUdpEndpoint(*session, clientEndpoint, message.Payload);
				continue;
			}

			if(!isUdpEndpointRegistered) {
				std::cout << "[Session " << session->GetSessionId() << "] Command ignored: UDP port is not registered." << std::endl;
				continue;
			}

			session->ProcessCommand(message.Type, message.Payload);
		}

		std::cout << "Session " << session->GetSessionId() << " control connection closed." << std::endl;

		sessionManager_.RemoveSession(session->GetSessionId());
		activeControlWorkerCount_.fetch_sub(1);
	}

	bool TcpControlServer::RegisterUdpEndpoint(ClientSession& session, const Common::Ipv4Endpoint& clientEndpoint, uint32_t udpPort) const
	{
		if (udpPort == 0 || udpPort > 65535) {
			std::cerr << "Invalid UDP port: " << udpPort << std::endl;
			return false;
		}

		session.RegisterUdpEndpoint(clientEndpoint.GetIpAddress(), static_cast<uint16_t>(udpPort));
		return true;
	}

} // namespace Server