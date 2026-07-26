#include "UdpReceiver.h"

#include "../Common/Ipv4Endpoint.h"
#include "../Common/Protocol.h"
#include "../Common/SocketRuntime.h"
#include "../Common/TcpSocket.h"

#include <cstdint>
#include <iomanip>
#include <iostream>
#include <string>

namespace {

	constexpr uint16_t ServerTcpPort = 5000;
	constexpr uint16_t ClientUdpPort = 6000;

	bool CreateServerEndpoint(Common::Ipv4Endpoint& endpoint)
	{
		if (!Common::Ipv4Endpoint::TryCreate("127.0.0.1", ServerTcpPort, endpoint)) {
			std::cerr << "Server endpoint creation failed." << std::endl;
			return false;
		}

		return true;
	}

	bool SendCommand(Common::TcpSocket& tcpSocket, Common::CommandType command, uint32_t payload = 0)
	{
		Common::ControlMessage message{};
		message.Type = command;
		message.Payload = payload;

		if (!tcpSocket.SendAll(&message, sizeof(message))) {
			std::cerr << "TCP command send failed: " << Common::TcpSocket::GetLastError() << std::endl;
			return false;
		}

		return true;
	}

	bool RunClientControlLoop(uint16_t clientUdpPort)
	{
		Client::UdpReceiver udpReceiver;

		if (!udpReceiver.Start(clientUdpPort)) {
			return false;
		}

		Common::Ipv4Endpoint serverEndpoint;
		if (!CreateServerEndpoint(serverEndpoint)) {
			return false;
		}

		Common::TcpSocket tcpSocket;
		if (!tcpSocket.Create()) {
			std::cerr << "TCP socket creation failed: " << Common::TcpSocket::GetLastError() << std::endl;
			return false;
		}
		
		if (!tcpSocket.Connect(serverEndpoint)) {
			std::cerr << "TCP connect failed: " << Common::TcpSocket::GetLastError() << std::endl;
			return false;
		}

		if (!SendCommand(tcpSocket, Common::CommandType::RegisterUdpPort, clientUdpPort)) {
			return false;
		}

		std::cout << "UDP port " << clientUdpPort << " registered." << std::endl;
		std::cout << "Commands: play, pause, stop, reset, 30, 60, stats, quit" << std::endl;

		std::string input;

		while (true)
		{
			std::cout << "> ";
			std::getline(std::cin, input);

			if (input == "quit") {
				return true;
			}

			if (input == "play") {
				if (!SendCommand(tcpSocket, Common::CommandType::Play)) {
					return false;
				}
			}
			else if (input == "pause") {
				if (!SendCommand(tcpSocket, Common::CommandType::Pause)) {
					return false;
				}
			}
			else if (input == "stop") {
				if (!SendCommand(tcpSocket, Common::CommandType::Stop)) {
					return false;
				}
			}
			else if (input == "reset") {
				if (!SendCommand(tcpSocket, Common::CommandType::Reset)) {
					return false;
				}
			}
			else if (input == "30") {
				if (!SendCommand(tcpSocket, Common::CommandType::SetRate, static_cast<uint32_t>(Common::DataRate::Hz30))) {
					return false;
				}

				udpReceiver.SetExpectedDataRate(Common::DataRate::Hz30);
			}
			else if (input == "60") {
				if (!SendCommand(tcpSocket, Common::CommandType::SetRate, static_cast<uint32_t>(Common::DataRate::Hz60))) {
					return false;
				}

				udpReceiver.SetExpectedDataRate(Common::DataRate::Hz60);
			}
			else if (input == "stats")
			{
				const Client::MetricsSnapshot metrics = udpReceiver.GetMetrics();

				std::cout << std::fixed << std::setprecision(2);
				std::cout << "Received: " << metrics.TotalReceivedCount << ", Loss: " << metrics.LossCount << ", Loss Rate: " << metrics.LossRate << "%" << ", Out of Order: " << metrics.OutOfOrderCount << std::endl;
				
				std::cout << "Average Interval: " << metrics.AverageReceiveIntervalMilliseconds << "ms" << ", Max Interval: " << metrics.MaxReceiveIntervalMilliseconds << "ms" << ", Average Deviation: " << metrics.AverageIntervalDeviationMilliseconds << "ms" << std::endl;

				std::cout << "Delayed Packets: " << metrics.DelayedPacketCount << " / " << metrics.IntervalSampleCount << ", Delayed Packet Rate: " << metrics.DelayedPacketRate << "%" << std::endl;
			}
			else {
				std::cout << "Unknown command." << std::endl;
			}
		}
	}

} // namespace

int main(int argc, char* argv[]) {
	uint16_t clientUdpPort = 6000;

	if (argc == 2)
	{
		const unsigned long parsedPort = std::strtoul(argv[1], nullptr, 10);

		if (parsedPort == 0 || parsedPort > 65535) {
			std::cerr << "Invalid UDP port." << std::endl;
			return 1;
		}

		clientUdpPort = static_cast<uint16_t>(parsedPort);
	}
	else if (argc > 2) {
		std::cerr << "Usage: Client.exe [udpPort]" << std::endl;
		return 1;
	}

	Common::SocketRuntime socketRuntime;

	if (!socketRuntime.IsInitialized()) {
		std::cerr << "Winsock initialization failed." << std::endl;
		return 1;
	}

	if (!RunClientControlLoop(clientUdpPort)) {
		std::cout << "Press Enter to exit.";
		std::cin.get();
		return 1;
	}

	return 0;
}