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
			uint8_t buffer[64]{};
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

			Common::EntityStatePayload payload;
			if (!Common::TryDeserializeEntityState(reader, payload)) {
				continue;
			}

			metricsCollector_.OnPacketReceived(payload.SequenceId, payload.Timestamp);
			entityWorld_.OnState(payload);
		}
	}

	void UdpReceiver::SetExpectedDataRate(Common::DataRate dataRate) {
		metricsCollector_.SetExpectedDataRate(dataRate);
	}

	void UdpReceiver::ResetReceiveTiming() {
		metricsCollector_.ResetReceiveTiming();
	}

	bool UdpReceiver::SendControlInput(const Common::Ipv4Endpoint& serverEndpoint, float throttle, float yaw)
	{
		Common::EntityControlInputPayload payload;
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
