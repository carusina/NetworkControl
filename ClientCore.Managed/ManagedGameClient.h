#pragma once

#include "ManagedTypes.h"

// using namespace System; 을 여기서 열지 않음 - ManagedTypes.h 주석 참고
// (네이티브 COM 헤더의 IServiceProvider와 충돌).

namespace Client { class GameClient; }

namespace ClientCoreManaged {

	// Client::GameClient(네이티브, ClientCore.lib)를 C#/WPF가 쓸 수 있게 감싼 래퍼.
	// 이벤트 없이 폴링 방식 - 호출부(WPF)가 타이머로 GetEntities()/GetMetrics()를 주기적으로 불러야 함.
	public ref class ManagedGameClient
	{
		public:
			ManagedGameClient();
			~ManagedGameClient();   // IDisposable::Dispose (결정적 해제)
			!ManagedGameClient();   // 파이널라이저 (Dispose를 안 불렀을 때의 안전망)

			bool Connect(System::String^ host, int tcpPort, int udpPort, int serverUdpPort);
			void Disconnect();

			bool Play();
			bool Pause();
			bool Stop();
			bool Reset();
			bool SetRate30();
			bool SetRate60();

			// throttle/yaw는 -1..1로 클램프됨 (네이티브 쪽에서 처리)
			bool SendControlInput(float throttle, float yaw);

			// 아직 첫 EntitySpawn을 못 받았으면 null
			System::Nullable<System::UInt32> GetMyEntityId();

			System::Collections::Generic::List<ManagedEntityInfo^>^ GetEntities();
			ManagedMetricsSnapshot^ GetMetrics();

		private:
			Client::GameClient* native_;
	};

} // namespace ClientCoreManaged
