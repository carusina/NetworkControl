#include "UdpReceiver.h"

#include "../Common/Config.h"
#include "../Common/Ipv4Endpoint.h"
#include "../Common/Protocol.h"
#include "../Common/SocketRuntime.h"
#include "../Common/TcpSocket.h"

#include <conio.h>
#include <Windows.h>

#include <chrono>
#include <cstdint>
#include <iomanip>
#include <iostream>
#include <string>
#include <thread>

namespace {

	constexpr const char* ConfigFilePath = "client.ini";
	constexpr auto StatsRefreshInterval = std::chrono::milliseconds(500);

	// 콘솔이 ANSI 커서 이동/삭제 시퀀스를 해석하도록 설정 (실시간 stats 화면 갱신에 사용)
	void EnableVirtualTerminalProcessing()
	{
		const HANDLE stdOutHandle = GetStdHandle(STD_OUTPUT_HANDLE);

		DWORD mode = 0;
		if (stdOutHandle != INVALID_HANDLE_VALUE && GetConsoleMode(stdOutHandle, &mode)) {
			SetConsoleMode(stdOutHandle, mode | ENABLE_VIRTUAL_TERMINAL_PROCESSING);
		}
	}

	void PrintMetrics(const Client::MetricsSnapshot& metrics)
	{
		std::cout << std::fixed << std::setprecision(2);
		std::cout << "Received: " << metrics.TotalReceivedCount << ", Loss: " << metrics.LossCount << ", Loss Rate: " << metrics.LossRate << "%" << ", Out of Order: " << metrics.OutOfOrderCount << std::endl;

		std::cout << "Average Interval: " << metrics.AverageReceiveIntervalMilliseconds << "ms" << ", Max Interval: " << metrics.MaxReceiveIntervalMilliseconds << "ms" << ", Average Deviation: " << metrics.AverageIntervalDeviationMilliseconds << "ms" << std::endl;

		std::cout << "Delayed Packets: " << metrics.DelayedPacketCount << " / " << metrics.IntervalSampleCount << ", Delayed Packet Rate: " << metrics.DelayedPacketRate << "%" << std::endl;
	}

	// stats 화면이 3줄을 출력하므로, 다음 프레임을 그리기 전에 그만큼 커서를 올려 지움
	void RunLiveStats(Client::UdpReceiver& udpReceiver)
	{
		std::cout << "Live stats - press any key to stop." << std::endl;

		bool isFirstFrame = true;

		while (!_kbhit())
		{
			if (!isFirstFrame) {
				std::cout << "\x1b[3A\x1b[0J";
			}
			isFirstFrame = false;

			PrintMetrics(udpReceiver.GetMetrics());

			std::this_thread::sleep_for(StatsRefreshInterval);
		}

		// 화면을 멈추는 데 사용한 키 입력이 다음 명령 프롬프트로 새어 들어가지 않도록 비움
		while (_kbhit()) {
			_getch();
		}
	}

	bool CreateServerEndpoint(const std::string& host, uint16_t port, Common::Ipv4Endpoint& endpoint)
	{
		if (!Common::Ipv4Endpoint::TryCreate(host, port, endpoint)) {
			std::cerr << "Server endpoint creation failed." << std::endl;
			return false;
		}

		return true;
	}

	bool SendCommand(Common::TcpSocket& tcpSocket, Common::CommandType command, uint32_t payload = 0)
	{
		Common::ControlMessage message{};
		message.Type = command;
		message.Payload = htonl(payload);

		if (!tcpSocket.SendAll(&message, sizeof(message))) {
			std::cerr << "TCP command send failed: " << Common::TcpSocket::GetLastError() << std::endl;
			return false;
		}

		return true;
	}

	bool RunClientControlLoop(const Common::ClientConfig& config)
	{
		Client::UdpReceiver udpReceiver;

		if (!udpReceiver.Start(config.UdpPort)) {
			return false;
		}

		Common::Ipv4Endpoint serverEndpoint;
		if (!CreateServerEndpoint(config.Host, config.TcpPort, serverEndpoint)) {
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

		if (!SendCommand(tcpSocket, Common::CommandType::RegisterUdpPort, config.UdpPort)) {
			return false;
		}

		std::cout << "UDP port " << config.UdpPort << " registered." << std::endl;
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
				RunLiveStats(udpReceiver);
			}
			else {
				std::cout << "Unknown command." << std::endl;
			}
		}
	}

} // namespace

int main(int argc, char* argv[]) {
	Common::ClientConfig config;

	if (Common::LoadClientConfig(ConfigFilePath, config)) {
		std::cout << "Loaded config from " << ConfigFilePath << "." << std::endl;
	}

	if (argc == 2)
	{
		const unsigned long parsedPort = std::strtoul(argv[1], nullptr, 10);

		if (parsedPort == 0 || parsedPort > 65535) {
			std::cerr << "Invalid UDP port." << std::endl;
			return 1;
		}

		config.UdpPort = static_cast<uint16_t>(parsedPort);
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

	EnableVirtualTerminalProcessing();

	if (!RunClientControlLoop(config)) {
		std::cout << "Press Enter to exit.";
		std::cin.get();
		return 1;
	}

	return 0;
}