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

			// 정상적으로 Stop()이 호출되지 않았는데 수신 루프가 끝났으면 true(=서버가 끊었다는 뜻) -
			// GameClient의 재접속 감시 스레드가 이 값으로 예기치 않은 연결 종료를 감지함
			bool HasFailedUnexpectedly() const;

		private:
			void ReceiveWorker();

		private:
			Common::TcpSocket& controlSocket_;
			EntityWorld& entityWorld_;
			std::thread workerThread_;
			std::atomic<bool> isRunning_{ false };
			std::atomic<bool> hasFailedUnexpectedly_{ false };
	};

} // namespace Client
