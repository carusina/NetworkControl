#include "SessionManager.h"
#include "TcpControlServer.h"
#include "UdpControlInputReceiver.h"
#include "UdpStreamingService.h"

#include "../Common/Config.h"
#include "../Common/SocketRuntime.h"

#include <conio.h>
#include <Windows.h>

#include <chrono>
#include <iomanip>
#include <iostream>
#include <string>
#include <thread>

namespace {

	constexpr const char* ConfigFilePath = "server.ini";
	constexpr auto EntitiesRefreshInterval = std::chrono::milliseconds(500);

	// 콘솔이 ANSI 커서 이동/삭제 시퀀스를 해석하도록 설정 (entities 실시간 갱신에 사용)
	void EnableVirtualTerminalProcessing()
	{
		const HANDLE stdOutHandle = GetStdHandle(STD_OUTPUT_HANDLE);

		DWORD mode = 0;
		if (stdOutHandle != INVALID_HANDLE_VALUE && GetConsoleMode(stdOutHandle, &mode)) {
			SetConsoleMode(stdOutHandle, mode | ENABLE_VIRTUAL_TERMINAL_PROCESSING);
		}
	}

	const char* ToString(Common::SessionState state)
	{
		switch (state)
		{
			case Common::SessionState::Playing: return "Playing";
			case Common::SessionState::Paused:  return "Paused";
			default:                            return "Stopped";
		}
	}

	// 세션마다 서버가 직접 계산해서 들고 있는 물리 상태(위치/헤딩/속도)를 콘솔에서 확인.
	// 세션 수에 따라 줄 수가 매번 달라지므로, 실제 출력한 줄 수를 반환(실시간 갱신에 씀)
	int PrintEntities(Server::SessionManager& sessionManager)
	{
		const auto sessions = sessionManager.GetSessionsSnapshot();

		std::cout << std::fixed << std::setprecision(2);
		std::cout << "Active sessions: " << sessions.size() << std::endl;

		for (const auto& session : sessions)
		{
			const Common::EntityStateEntry entry = session->BuildEntityStateEntry();
			const auto dataRateHz = static_cast<uint32_t>(session->GetDataRate());

			std::cout << "  [" << entry.EntityId << "]"
				<< " " << ToString(session->GetState())
				<< " " << dataRateHz << "Hz"
				<< " Pos(" << entry.PositionX << ", " << entry.PositionY << ")"
				<< " Heading " << entry.Heading
				<< " Vel(" << entry.VelocityX << ", " << entry.VelocityY << ")"
				<< std::endl;
		}

		return 1 + static_cast<int>(sessions.size());
	}

	// printFrame은 화면을 한 번 그리고 출력한 줄 수를 반환하는 함수 - Client의 RunLiveView와 동일한 패턴
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

			std::this_thread::sleep_for(EntitiesRefreshInterval);
		}

		// 화면을 멈추는 데 사용한 키 입력이 다음 명령 프롬프트로 새어 들어가지 않도록 비움
		while (_kbhit()) {
			_getch();
		}
	}

} // namespace

int main() {
	EnableVirtualTerminalProcessing();

	Common::SocketRuntime socketRuntime;

	if (!socketRuntime.IsInitialized()) {
		std::cerr << "Winsock initialization failed." << std::endl;
		return 1;
	}

	Common::ServerConfig config;
	if (Common::LoadServerConfig(ConfigFilePath, config)) {
		std::cout << "Loaded config from " << ConfigFilePath << "." << std::endl;
	}

	Server::SessionManager sessionManager;
	Server::UdpStreamingService streamingService(sessionManager);
	Server::UdpControlInputReceiver controlInputReceiver(sessionManager);
	Server::TcpControlServer controlServer(sessionManager);

	if (!streamingService.Start() ||
		!controlInputReceiver.Start(config.UdpPort) ||
		!controlServer.Start(config.TcpPort))
	{
		return 1;
	}

	std::cout << "Commands: entities, quit" << std::endl;

	std::string input;
	while (std::getline(std::cin, input))
	{
		if (input == "quit") {
			break;
		}
		else if (input == "entities") {
			RunLiveView([&sessionManager]() { return PrintEntities(sessionManager); });
		}
		else if (!input.empty()) {
			std::cout << "Unknown command." << std::endl;
		}
	}

	controlServer.Stop();
	controlInputReceiver.Stop();
	streamingService.Stop();

	return 0;
}