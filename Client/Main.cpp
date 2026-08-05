#include "../ClientCore/GameClient.h"

#include "../Common/Config.h"
#include "../Common/SocketRuntime.h"

#include <conio.h>
#include <Windows.h>

#include <chrono>
#include <cstdint>
#include <cstdlib>
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

	// 화면에 출력한 줄 수를 반환 - 다음 프레임을 그리기 전에 그만큼 커서를 올려 지우는 데 씀
	int PrintMetrics(const Client::MetricsSnapshot& metrics)
	{
		std::cout << std::fixed << std::setprecision(2);
		std::cout << "Elapsed: " << metrics.ElapsedSeconds << "s" << std::endl;

		std::cout << "Received: " << metrics.TotalReceivedCount << ", Loss: " << metrics.LossCount << ", Loss Rate: " << metrics.LossRate << "%" << ", Out of Order: " << metrics.OutOfOrderCount << std::endl;

		std::cout << "Average Interval: " << metrics.AverageReceiveIntervalMilliseconds << "ms" << ", Max Interval: " << metrics.MaxReceiveIntervalMilliseconds << "ms" << ", Average Deviation: " << metrics.AverageIntervalDeviationMilliseconds << "ms" << std::endl;

		std::cout << "Delayed Packets: " << metrics.DelayedPacketCount << " / " << metrics.IntervalSampleCount << ", Delayed Packet Rate: " << metrics.DelayedPacketRate << "%" << std::endl;

		std::cout << "Latency: avg " << metrics.AverageLatencyMilliseconds << "ms, min " << metrics.MinLatencyMilliseconds << "ms, max " << metrics.MaxLatencyMilliseconds << "ms (" << metrics.LatencySampleCount << " samples, same-machine only)" << std::endl;

		return 5;
	}

	// 알려진 엔티티 수에 따라 줄 수가 매번 달라지므로, 실제 출력한 줄 수를 반환
	int PrintEntities(const Client::GameClient& gameClient)
	{
		uint32_t myEntityId = 0;
		const bool hasMyEntityId = gameClient.TryGetMyEntityId(myEntityId);
		const auto entities = gameClient.GetEntities();

		std::cout << std::fixed << std::setprecision(2);
		std::cout << "Known entities: " << entities.size() << std::endl;

		for (const auto& entity : entities)
		{
			std::cout << "  [" << entity.EntityId << "]"
				<< ((hasMyEntityId && entity.EntityId == myEntityId) ? " (me)" : "")
				<< " Pos(" << entity.PositionX << ", " << entity.PositionY << ")"
				<< " Heading " << entity.Heading
				<< " Vel(" << entity.VelocityX << ", " << entity.VelocityY << ")"
				<< std::endl;
		}

		return 1 + static_cast<int>(entities.size());
	}

	// printFrame은 화면을 한 번 그리고 출력한 줄 수를 반환하는 함수 - stats/entities가 공유하는 실시간 갱신 루프
	template <typename PrintFrameFn>
	void RunLiveView(PrintFrameFn printFrame)
	{
		std::cout << "Live view - press any key to stop." << std::endl;

		bool isFirstFrame = true;
		int previousLineCount = 0;

		while (!_kbhit())
		{
			if (!isFirstFrame) {
				std::cout << "\x1b[" << previousLineCount << "A\x1b[0J";
			}
			isFirstFrame = false;

			previousLineCount = printFrame();

			std::this_thread::sleep_for(StatsRefreshInterval);
		}

		// 화면을 멈추는 데 사용한 키 입력이 다음 명령 프롬프트로 새어 들어가지 않도록 비움
		while (_kbhit()) {
			_getch();
		}
	}

	// "thrust 0.5" / "yaw -0.2" 같은 명령에서 값을 파싱. 실패하면 false
	bool TryParseFloatArgument(const std::string& input, size_t prefixLength, float& value)
	{
		try {
			value = std::stof(input.substr(prefixLength));
			return true;
		}
		catch (const std::exception&) {
			return false;
		}
	}

	// 명령 전송 실패는 이제 프로그램 종료가 아니라 "재접속 중이니 잠시 후 다시" 안내로 처리
	void ReportCommandFailure(const Client::GameClient& gameClient)
	{
		if (gameClient.GetConnectionState() == Client::ConnectionState::Reconnecting) {
			std::cout << "서버와의 연결이 끊겨 재접속을 시도하는 중입니다. 잠시 후 다시 시도해주세요." << std::endl;
		}
		else {
			std::cout << "명령 전송에 실패했습니다." << std::endl;
		}
	}

	bool RunClientControlLoop(const Common::ClientConfig& config)
	{
		Client::GameClient gameClient;

		if (!gameClient.Connect(config)) {
			return false;
		}

		std::cout << "UDP port " << config.UdpPort << " registered." << std::endl;
		std::cout << "Commands: play, pause, stop, reset, 30, 60, thrust <-1..1>, yaw <-1..1>, stats, entities, quit" << std::endl;

		float lastThrottle = 0.0f;
		float lastYaw = 0.0f;

		// 접속 후에는 예기치 않게 끊겨도 백그라운드에서 자동 재접속을 시도하므로, 명령 프롬프트로
		// 돌아올 때마다 상태 변화를 감지해서 한 줄 안내만 하고 REPL 자체는 계속 살려둠
		Client::ConnectionState lastKnownState = Client::ConnectionState::Connected;

		std::string input;

		while (true)
		{
			const Client::ConnectionState currentState = gameClient.GetConnectionState();
			if (currentState != lastKnownState)
			{
				if (currentState == Client::ConnectionState::Reconnecting) {
					std::cout << "서버와의 연결이 끊겼습니다. 재접속을 시도합니다..." << std::endl;
				}
				else if (currentState == Client::ConnectionState::Connected) {
					std::cout << "서버에 다시 연결되었습니다." << std::endl;
				}
				lastKnownState = currentState;
			}

			std::cout << "> ";
			std::getline(std::cin, input);

			if (input == "quit") {
				return true;
			}

			if (input == "play") {
				if (!gameClient.Play()) {
					ReportCommandFailure(gameClient);
				}
			}
			else if (input == "pause") {
				if (!gameClient.Pause()) {
					ReportCommandFailure(gameClient);
				}
			}
			else if (input == "stop") {
				if (!gameClient.Stop()) {
					ReportCommandFailure(gameClient);
				}
			}
			else if (input == "reset") {
				if (!gameClient.Reset()) {
					ReportCommandFailure(gameClient);
				}
				else {
					lastThrottle = 0.0f;
					lastYaw = 0.0f;
				}
			}
			else if (input == "30") {
				if (!gameClient.SetRate(Common::DataRate::Hz30)) {
					ReportCommandFailure(gameClient);
				}
			}
			else if (input == "60") {
				if (!gameClient.SetRate(Common::DataRate::Hz60)) {
					ReportCommandFailure(gameClient);
				}
			}
			else if (input.rfind("thrust ", 0) == 0)
			{
				float value = 0.0f;
				if (!TryParseFloatArgument(input, 7, value)) {
					std::cout << "Usage: thrust <-1..1>" << std::endl;
				}
				else {
					lastThrottle = value;
					if (!gameClient.SendControlInput(lastThrottle, lastYaw)) {
						std::cout << "Not registered yet - try again in a moment." << std::endl;
					}
				}
			}
			else if (input.rfind("yaw ", 0) == 0)
			{
				float value = 0.0f;
				if (!TryParseFloatArgument(input, 4, value)) {
					std::cout << "Usage: yaw <-1..1>" << std::endl;
				}
				else {
					lastYaw = value;
					if (!gameClient.SendControlInput(lastThrottle, lastYaw)) {
						std::cout << "Not registered yet - try again in a moment." << std::endl;
					}
				}
			}
			else if (input == "entities")
			{
				RunLiveView([&gameClient]() { return PrintEntities(gameClient); });
			}
			else if (input == "stats")
			{
				RunLiveView([&gameClient]() { return PrintMetrics(gameClient.GetMetrics()); });
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
