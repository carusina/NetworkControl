#include "UdpReceiver.h"

#include "../Common/BinarySerializer.h"

namespace Client {

	UdpReceiver::UdpReceiver(EntityWorld& entityWorld)
		: entityWorld_(entityWorld) { }

	UdpReceiver::~UdpReceiver() {
		Stop();
	}

	bool UdpReceiver::Start(uint16_t port)
	{
		Stop();
		isActive_ = false; // 새 세션은 서버 기본값(Stopped)과 맞춰 항상 비활성 상태로 시작

		Common::Ipv4Endpoint endpoint;
		if (!Common::Ipv4Endpoint::TryCreate("0.0.0.0", port, endpoint) ||
			!udpSocket_.Create() ||
			!udpSocket_.Bind(endpoint) ||
			!udpSocket_.SetReceiveTimeout(100))
		{
			udpSocket_.Close();
			return false;
		}

		isRunning_ = true;
		workerThread_ = std::thread(&UdpReceiver::ReceiveWorker, this);
		return true;
	}

	void UdpReceiver::Stop()
	{
		isRunning_ = false;

		if (workerThread_.joinable()) {
			workerThread_.join();
		}

		udpSocket_.Close();
	}

	void UdpReceiver::ResetMetrics() {
		metricsCollector_.Reset();
	}

	MetricsSnapshot UdpReceiver::GetMetrics() const {
		return metricsCollector_.GetSnapshot();
	}

	void UdpReceiver::ReceiveWorker()
	{
		while (isRunning_)
		{
			// 한 패킷에 여러 엔티티가 묶여 오므로 넉넉하게 잡음 (엔티티당 24바이트 기준 60개 이상 여유)
			uint8_t buffer[2048]{};
			Common::Ipv4Endpoint sender;
			const int received = udpSocket_.ReceiveFrom(buffer, sizeof(buffer), sender);

			if (received <= 0) {
				continue;
			}

			Common::BinaryReader reader(buffer, static_cast<size_t>(received));

			Common::MessageType type{};
			if (!Common::TryDeserializeHeader(reader, type) || type != Common::MessageType::EntityState) {
				continue;
			}

			Common::EntityStateBatchPayload batch;
			if (!Common::TryDeserializeEntityStateBatch(reader, batch)) {
				continue;
			}

			// 로컬에서 Pause/Stop을 반영한 뒤 뒤늦게 도착한 스트래글러 패킷은 여기서 조용히 버림
			if (!isActive_) {
				continue;
			}

			metricsCollector_.OnPacketReceived(batch.SequenceId, batch.Timestamp);

			for (const auto& entry : batch.Entities) {
				entityWorld_.OnState(entry);
			}
		}
	}

	void UdpReceiver::SetExpectedDataRate(Common::DataRate dataRate) {
		metricsCollector_.SetExpectedDataRate(dataRate);
	}

	void UdpReceiver::OnPlay() {
		isActive_ = true;
		metricsCollector_.OnPlay();
	}

	void UdpReceiver::OnPause() {
		isActive_ = false;
		metricsCollector_.OnPause();
	}

	void UdpReceiver::OnStop() {
		isActive_ = false;
	}

	bool UdpReceiver::SendControlInput(const Common::Ipv4Endpoint& serverEndpoint, uint32_t entityId, float throttle, float yaw)
	{
		Common::EntityControlInputPayload payload;
		payload.EntityId = entityId;
		payload.SequenceId = nextControlInputSequenceId_.fetch_add(1) + 1;
		payload.Throttle = throttle;
		payload.Yaw = yaw;

		Common::BinaryWriter writer;
		Common::SerializeHeader(writer, Common::MessageType::EntityControlInput);
		Common::SerializeEntityControlInput(writer, payload);

		const auto& data = writer.Data();
		const int sent = udpSocket_.SendTo(data.data(), static_cast<int>(data.size()), serverEndpoint);

		return sent == static_cast<int>(data.size());
	}

} // namespace Client
