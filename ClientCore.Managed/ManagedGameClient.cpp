#include "ManagedGameClient.h"

#include "../ClientCore/GameClient.h"
#include "../Common/Config.h"
#include "../Common/SocketRuntime.h"

#include <msclr/marshal_cppstd.h>

// using namespace System; 을 여기서도 열지 않음 - ManagedTypes.h 주석 참고.
// System:: / System::Collections::Generic:: 을 계속 완전한 이름으로 씀.

namespace ClientCoreManaged {

	namespace {

		// 콘솔 앱의 main()이 하던 WSAStartup/WSACleanup을, 이 DLL에서는 프로세스 전체에서
		// 한 번만 초기화되는 static 지역 변수로 대신함 (ManagedGameClient가 여러 개 만들어져도 안전).
		// 이 함수가 SocketRuntime.obj를 링크에 끌어들여서, 그 안의
		// #pragma comment(lib, "Ws2_32.lib")도 같이 적용되게 만듦.
		Common::SocketRuntime& GetSocketRuntime()
		{
			static Common::SocketRuntime instance;
			return instance;
		}

	} // namespace

	ManagedGameClient::ManagedGameClient()
		: native_(new Client::GameClient())
	{
		GetSocketRuntime();
	}

	ManagedGameClient::~ManagedGameClient() {
		this->!ManagedGameClient();
	}

	ManagedGameClient::!ManagedGameClient() {
		delete native_;
		native_ = nullptr;
	}

	bool ManagedGameClient::Connect(System::String^ host, int tcpPort, int udpPort, int serverUdpPort)
	{
		if (!GetSocketRuntime().IsInitialized()) {
			return false;
		}

		Common::ClientConfig config;
		config.Host = msclr::interop::marshal_as<std::string>(host);
		config.TcpPort = static_cast<uint16_t>(tcpPort);
		config.UdpPort = static_cast<uint16_t>(udpPort);
		config.ServerUdpPort = static_cast<uint16_t>(serverUdpPort);

		return native_->Connect(config);
	}

	void ManagedGameClient::Disconnect() {
		native_->Disconnect();
	}

	bool ManagedGameClient::Play()  { return native_->Play();  }
	bool ManagedGameClient::Pause() { return native_->Pause(); }
	bool ManagedGameClient::Stop()  { return native_->Stop();  }
	bool ManagedGameClient::Reset() { return native_->Reset(); }

	bool ManagedGameClient::SetRate30() { return native_->SetRate(Common::DataRate::Hz30); }
	bool ManagedGameClient::SetRate60() { return native_->SetRate(Common::DataRate::Hz60); }

	bool ManagedGameClient::SendControlInput(float throttle, float yaw) {
		return native_->SendControlInput(throttle, yaw);
	}

	System::Nullable<System::UInt32> ManagedGameClient::GetMyEntityId()
	{
		uint32_t entityId = 0;
		if (native_->TryGetMyEntityId(entityId)) {
			return System::Nullable<System::UInt32>(entityId);
		}

		return System::Nullable<System::UInt32>();
	}

	System::Collections::Generic::List<ManagedEntityInfo^>^ ManagedGameClient::GetEntities()
	{
		auto result = gcnew System::Collections::Generic::List<ManagedEntityInfo^>();

		for (const auto& entity : native_->GetEntities())
		{
			auto managedEntity = gcnew ManagedEntityInfo();
			managedEntity->EntityId = entity.EntityId;
			managedEntity->PositionX = entity.PositionX;
			managedEntity->PositionY = entity.PositionY;
			managedEntity->Heading = entity.Heading;
			managedEntity->VelocityX = entity.VelocityX;
			managedEntity->VelocityY = entity.VelocityY;

			result->Add(managedEntity);
		}

		return result;
	}

	ManagedMetricsSnapshot^ ManagedGameClient::GetMetrics()
	{
		const Client::MetricsSnapshot snapshot = native_->GetMetrics();

		auto managed = gcnew ManagedMetricsSnapshot();
		managed->ElapsedSeconds = snapshot.ElapsedSeconds;
		managed->TotalReceivedCount = snapshot.TotalReceivedCount;
		managed->LossCount = snapshot.LossCount;
		managed->OutOfOrderCount = snapshot.OutOfOrderCount;
		managed->LossRate = snapshot.LossRate;

		managed->IntervalSampleCount = snapshot.IntervalSampleCount;
		managed->AverageReceiveIntervalMilliseconds = snapshot.AverageReceiveIntervalMilliseconds;
		managed->MaxReceiveIntervalMilliseconds = snapshot.MaxReceiveIntervalMilliseconds;
		managed->AverageIntervalDeviationMilliseconds = snapshot.AverageIntervalDeviationMilliseconds;

		managed->DelayedPacketCount = snapshot.DelayedPacketCount;
		managed->DelayedPacketRate = snapshot.DelayedPacketRate;

		managed->LatencySampleCount = snapshot.LatencySampleCount;
		managed->AverageLatencyMilliseconds = snapshot.AverageLatencyMilliseconds;
		managed->MinLatencyMilliseconds = snapshot.MinLatencyMilliseconds;
		managed->MaxLatencyMilliseconds = snapshot.MaxLatencyMilliseconds;

		return managed;
	}

} // namespace ClientCoreManaged
