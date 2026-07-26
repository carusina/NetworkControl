#include "UdpReceiver.h"

#include "../Common/Protocol.h"

namespace Client {

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
			Common::Packet packet{};
			Common::Ipv4Endpoint sender;
			const int received = udpSocket_.ReceiveFrom(&packet, sizeof(packet), sender);

			if (received == sizeof(packet)) {
				metricsCollector_.OnPacketReceived(packet.SequenceId);
			}
		}
	}

	void UdpReceiver::SetExpectedDataRate(Common::DataRate dataRate) {
		metricsCollector_.SetExpectedDataRate(dataRate);
	}

	void UdpReceiver::ResetReceiveTiming() {
		metricsCollector_.ResetReceiveTiming();
	}

} // namespace Client