#include "GameClient.h"

#include "../Common/BinarySerializer.h"

#include <iostream>

namespace Client {

	namespace {

		float ClampToUnitRange(float value) {
			if (value < -1.0f) return -1.0f;
			if (value > 1.0f) return 1.0f;
			return value;
		}

	} // namespace

	GameClient::GameClient()
		: udpReceiver_(entityWorld_) { }

	bool GameClient::Connect(const Common::ClientConfig& config)
	{
		if (isConnected_) {
			Disconnect();
		}

		if (!udpReceiver_.Start(config.UdpPort)) {
			return false;
		}

		Common::Ipv4Endpoint serverTcpEndpoint;
		if (!Common::Ipv4Endpoint::TryCreate(config.Host, config.TcpPort, serverTcpEndpoint)) {
			std::cerr << "Server endpoint creation failed." << std::endl;
			return false;
		}

		if (!Common::Ipv4Endpoint::TryCreate(config.Host, config.ServerUdpPort, serverUdpEndpoint_)) {
			std::cerr << "Server UDP endpoint creation failed." << std::endl;
			return false;
		}

		if (!tcpSocket_.Create()) {
			std::cerr << "TCP socket creation failed: " << Common::TcpSocket::GetLastError() << std::endl;
			return false;
		}

		if (!tcpSocket_.Connect(serverTcpEndpoint)) {
			std::cerr << "TCP connect failed: " << Common::TcpSocket::GetLastError() << std::endl;
			return false;
		}

		if (!SendRegisterUdpPort(config.UdpPort)) {
			return false;
		}

		tcpMessageReceiver_ = std::make_unique<TcpMessageReceiver>(tcpSocket_, entityWorld_);
		tcpMessageReceiver_->Start();

		isConnected_ = true;
		return true;
	}

	void GameClient::Disconnect()
	{
		if (tcpMessageReceiver_) {
			tcpMessageReceiver_->Stop();
			tcpMessageReceiver_.reset();
		}

		udpReceiver_.Stop();
		tcpSocket_.Close();
		isConnected_ = false;
	}

	bool GameClient::Play()
	{
		if (!isConnected_ || !SendHeaderOnly(Common::MessageType::Play)) {
			return false;
		}

		udpReceiver_.ResetReceiveTiming();
		return true;
	}

	bool GameClient::Pause()
	{
		if (!isConnected_ || !SendHeaderOnly(Common::MessageType::Pause)) {
			return false;
		}

		udpReceiver_.ResetReceiveTiming();
		return true;
	}

	bool GameClient::Stop()
	{
		// 리셋되기 전의 마지막 통계를 서버에 실어 보냄 (서버는 이 값을 스스로 계산할 수 없음)
		if (!isConnected_ || !SendStop(udpReceiver_.GetMetrics())) {
			return false;
		}

		// Stop은 서버 쪽 엔티티/시퀀스가 전부 초기화되므로, 통계도 전부 초기화(ResetReceiveTiming보다 강함)
		udpReceiver_.ResetMetrics();
		return true;
	}

	bool GameClient::Reset()
	{
		if (!isConnected_ || !SendHeaderOnly(Common::MessageType::Reset)) {
			return false;
		}

		udpReceiver_.ResetMetrics();
		return true;
	}

	bool GameClient::SetRate(Common::DataRate dataRate)
	{
		if (!isConnected_ || !SendSetRate(static_cast<uint32_t>(dataRate))) {
			return false;
		}

		udpReceiver_.SetExpectedDataRate(dataRate);
		return true;
	}

	bool GameClient::SendControlInput(float throttle, float yaw)
	{
		uint32_t myEntityId = 0;
		if (!isConnected_ || !entityWorld_.TryGetMyEntityId(myEntityId)) {
			return false;
		}

		return udpReceiver_.SendControlInput(serverUdpEndpoint_, myEntityId, ClampToUnitRange(throttle), ClampToUnitRange(yaw));
	}

	bool GameClient::TryGetMyEntityId(uint32_t& entityId) const {
		return entityWorld_.TryGetMyEntityId(entityId);
	}

	std::vector<EntityInfo> GameClient::GetEntities() const {
		return entityWorld_.GetSnapshot();
	}

	MetricsSnapshot GameClient::GetMetrics() const {
		return udpReceiver_.GetMetrics();
	}

	bool GameClient::SendMessage(const Common::BinaryWriter& writer)
	{
		const auto& data = writer.Data();

		if (!tcpSocket_.SendAll(data.data(), data.size())) {
			std::cerr << "TCP command send failed: " << Common::TcpSocket::GetLastError() << std::endl;
			return false;
		}

		return true;
	}

	bool GameClient::SendHeaderOnly(Common::MessageType type)
	{
		Common::BinaryWriter writer;
		Common::SerializeHeader(writer, type);
		return SendMessage(writer);
	}

	bool GameClient::SendRegisterUdpPort(uint16_t port)
	{
		Common::BinaryWriter writer;
		Common::SerializeHeader(writer, Common::MessageType::RegisterUdpPort);

		Common::RegisterUdpPortPayload payload;
		payload.Port = port;
		Common::SerializeRegisterUdpPort(writer, payload);

		return SendMessage(writer);
	}

	bool GameClient::SendSetRate(uint32_t dataRateHz)
	{
		Common::BinaryWriter writer;
		Common::SerializeHeader(writer, Common::MessageType::SetRate);

		Common::SetRatePayload payload;
		payload.DataRateHz = dataRateHz;
		Common::SerializeSetRate(writer, payload);

		return SendMessage(writer);
	}

	bool GameClient::SendStop(const MetricsSnapshot& metrics)
	{
		Common::BinaryWriter writer;
		Common::SerializeHeader(writer, Common::MessageType::Stop);

		Common::StopPayload payload;
		payload.TotalReceivedCount = metrics.TotalReceivedCount;
		payload.LossCount = metrics.LossCount;
		payload.OutOfOrderCount = metrics.OutOfOrderCount;
		payload.LossRate = metrics.LossRate;

		payload.IntervalSampleCount = metrics.IntervalSampleCount;
		payload.AverageReceiveIntervalMilliseconds = metrics.AverageReceiveIntervalMilliseconds;
		payload.MaxReceiveIntervalMilliseconds = metrics.MaxReceiveIntervalMilliseconds;
		payload.AverageIntervalDeviationMilliseconds = metrics.AverageIntervalDeviationMilliseconds;

		payload.DelayedPacketCount = metrics.DelayedPacketCount;
		payload.DelayedPacketRate = metrics.DelayedPacketRate;

		payload.LatencySampleCount = metrics.LatencySampleCount;
		payload.AverageLatencyMilliseconds = metrics.AverageLatencyMilliseconds;
		payload.MinLatencyMilliseconds = metrics.MinLatencyMilliseconds;
		payload.MaxLatencyMilliseconds = metrics.MaxLatencyMilliseconds;

		Common::SerializeStop(writer, payload);

		return SendMessage(writer);
	}

} // namespace Client
