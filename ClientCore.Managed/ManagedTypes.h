#pragma once

// System을 using namespace로 열지 않음 - servprov.h 등 네이티브 COM 헤더의
// IServiceProvider와 System::IServiceProvider가 이름이 겹쳐서 충돌남 (C3699/C2371).
// 그래서 여기서는 매니지드 타입을 전부 System:: 붙여서 완전한 이름으로 씀.

namespace ClientCoreManaged {

	// Client::EntityInfo(네이티브)를 C#에서 바인딩하기 좋은 모양으로 그대로 옮긴 POCO
	public ref class ManagedEntityInfo
	{
		public:
			property System::UInt32 EntityId;
			property System::Single PositionX;
			property System::Single PositionY;
			property System::Single Heading;
			property System::Single VelocityX;
			property System::Single VelocityY;
	};

	// Client::MetricsSnapshot(네이티브)를 그대로 옮긴 POCO
	public ref class ManagedMetricsSnapshot
	{
		public:
			property System::Double ElapsedSeconds;

			property System::UInt64 TotalReceivedCount;
			property System::UInt64 LossCount;
			property System::UInt64 OutOfOrderCount;
			property System::Double LossRate;

			property System::UInt64 IntervalSampleCount;
			property System::Double AverageReceiveIntervalMilliseconds;
			property System::Double MaxReceiveIntervalMilliseconds;
			property System::Double AverageIntervalDeviationMilliseconds;

			property System::UInt64 DelayedPacketCount;
			property System::Double DelayedPacketRate;

			property System::UInt64 LatencySampleCount;
			property System::Double AverageLatencyMilliseconds;
			property System::Double MinLatencyMilliseconds;
			property System::Double MaxLatencyMilliseconds;
	};

} // namespace ClientCoreManaged
