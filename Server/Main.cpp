#include "SessionManager.h"
#include "TcpControlServer.h"
#include "UdpStreamingService.h"

#include "../Common/Config.h"
#include "../Common/SocketRuntime.h"

#include <iostream>
#include <string>

namespace {

	constexpr const char* ConfigFilePath = "server.ini";

} // namespace

int main() {
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
	Server::TcpControlServer controlServer(sessionManager);

	if (!streamingService.Start() || !controlServer.Start(config.TcpPort)) {
		return 1;
	}

	std::cout << "Type quit to stop the server." << std::endl;

	std::string input;
	while (std::getline(std::cin, input))
	{
		if (input == "quit") {
			break;
		}
	}

	controlServer.Stop();
	streamingService.Stop();

	return 0;
}