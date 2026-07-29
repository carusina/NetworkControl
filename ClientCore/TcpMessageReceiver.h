#pragma once

#include "EntityWorld.h"

#include "../Common/TcpSocket.h"

#include <atomic>
#include <thread>

namespace Client {

	// TCP 제어 소켓에서 서버가 보내는 EntitySpawn/EntityDespawn을 계속 읽는 백그라운드 스레드
	// (제어 명령 전송은 이 클래스가 아니라 기존 흐름이 같은 소켓에 계속 씀 - TCP는 전이중이라 안전)
	class TcpMessageReceiver {
		public:
			TcpMessageReceiver(Common::TcpSocket& controlSocket, EntityWorld& entityWorld);
			~TcpMessageReceiver();

			TcpMessageReceiver(const TcpMessageReceiver&) = delete;
			TcpMessageReceiver& operator=(const TcpMessageReceiver&) = delete;

			void Start();
			void Stop();

		private:
			void ReceiveWorker();

		private:
			Common::TcpSocket& controlSocket_;
			EntityWorld& entityWorld_;
			std::thread workerThread_;
			std::atomic<bool> isRunning_{ false };
	};

} // namespace Client
