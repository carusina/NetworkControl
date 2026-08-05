#include "GameClient.h"

#include "../Common/BinarySerializer.h"

#include <chrono>
#include <iostream>
#include <thread>

namespace Client {

	namespace {

		float ClampToUnitRange(float value) {
			if (value < -1.0f) return -1.0f;
			if (value > 1.0f) return 1.0f;
			return value;
		}

		// 예기치 않게 끊긴 뒤 재접속을 시도하는 간격
		constexpr auto ReconnectInterval = std::chrono::milliseconds(2000);

		// 워치독이 연결 상태를 다시 확인하는 주기 - Disconnect()가 이 스레드를 join하기까지
		// 기다리는 최대 시간과도 직결되므로 너무 길게 잡지 않음
		constexpr auto WatchdogPollInterval = std::chrono::milliseconds(300);

	} // namespace

	GameClient::GameClient()
		: udpReceiver_(entityWorld_) { }

	GameClient::~GameClient() {
		Disconnect();
	}

	bool GameClient::Connect(const Common::ClientConfig& config)
	{
		std::lock_guard<std::mutex> lock(connectionMutex_);

		if (connectionState_ != ConnectionState::Disconnected) {
			TeardownConnectionLocked();
		}

		const bool connected = ConnectLocked(config);

		if (connected)
		{
			lastConfig_ = config;

			// 이미 감시 스레드가 돌고 있으면(재접속 성공 등) 새로 안 만듦 - Disconnect()가
			// 호출될 때까지 스레드 하나가 이 GameClient의 수명 내내 재사용됨
			if (!isWatchdogRunning_.exchange(true)) {
				watchdogThread_ = std::thread(&GameClient::WatchdogWorker, this);
			}
		}

		return connected;
	}

	void GameClient::Disconnect()
	{
		// 워치독을 먼저 멈춤 - connectionMutex_를 잡기 전에 join해야, 워치독이 그 락을
		// 필요로 하는 타이밍이어도 서로 안 막힘
		isWatchdogRunning_ = false;
		if (watchdogThread_.joinable()) {
			watchdogThread_.join();
		}

		std::lock_guard<std::mutex> lock(connectionMutex_);
		TeardownConnectionLocked();
		connectionState_ = ConnectionState::Disconnected;
	}

	bool GameClient::IsConnected() const {
		return connectionState_ == ConnectionState::Connected;
	}

	ConnectionState GameClient::GetConnectionState() const {
		return connectionState_;
	}

	bool GameClient::ConnectLocked(const Common::ClientConfig& config)
	{
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

		// 재접속인 경우 이전 세션(예전 EntityId 포함)의 흔적을 지움 - 서버는 재접속을 완전히
		// 새 세션으로 취급하므로(EntityId가 SessionId 재사용), 끊기기 전 상태로 이어지지 않음
		entityWorld_.Clear();

		tcpMessageReceiver_ = std::make_unique<TcpMessageReceiver>(tcpSocket_, entityWorld_);
		tcpMessageReceiver_->Start();

		connectionState_ = ConnectionState::Connected;
		return true;
	}

	void GameClient::TeardownConnectionLocked()
	{
		if (tcpMessageReceiver_) {
			tcpMessageReceiver_->Stop();
			tcpMessageReceiver_.reset();
		}

		udpReceiver_.Stop();
		tcpSocket_.Close();
	}

	void GameClient::WatchdogWorker()
	{
		auto nextRetryTime = std::chrono::steady_clock::time_point{};

		while (isWatchdogRunning_)
		{
			std::this_thread::sleep_for(WatchdogPollInterval);

			if (!isWatchdogRunning_) {
				break;
			}

			std::lock_guard<std::mutex> lock(connectionMutex_);

			if (connectionState_ == ConnectionState::Connected)
			{
				// TcpMessageReceiver의 수신 루프가 우리가 Stop()을 부른 게 아닌데 끝났으면
				// 서버가 연결을 끊었다는 뜻 - 정리하고 재접속 시도 상태로 전환
				if (tcpMessageReceiver_ && tcpMessageReceiver_->HasFailedUnexpectedly())
				{
					TeardownConnectionLocked();
					connectionState_ = ConnectionState::Reconnecting;
					nextRetryTime = std::chrono::steady_clock::now() + ReconnectInterval;
				}
			}
			else if (connectionState_ == ConnectionState::Reconnecting)
			{
				if (std::chrono::steady_clock::now() >= nextRetryTime)
				{
					if (!ConnectLocked(lastConfig_)) {
						nextRetryTime = std::chrono::steady_clock::now() + ReconnectInterval;
					}
				}
			}
		}
	}

	bool GameClient::Play()
	{
		if (!IsConnected() || !SendHeaderOnly(Common::MessageType::Play)) {
			return false;
		}

		udpReceiver_.OnPlay();
		return true;
	}

	bool GameClient::Pause()
	{
		if (!IsConnected() || !SendHeaderOnly(Common::MessageType::Pause)) {
			return false;
		}

		udpReceiver_.OnPause();
		return true;
	}

	bool GameClient::Stop()
	{
		if (!IsConnected()) {
			return false;
		}

		// 서버가 아직 이 Stop을 처리하기 전에 이미 날아오고 있던 EntityState가 뒤늦게 도착해도
		// 무시하도록, 통계를 건드리기 전에 먼저 수신 비활성화부터 함
		udpReceiver_.OnStop();

		// 리셋되기 전의 마지막 통계를 서버에 실어 보냄 (서버는 이 값을 스스로 계산할 수 없음)
		if (!SendStop(udpReceiver_.GetMetrics())) {
			return false;
		}

		// Stop은 서버 쪽 엔티티/시퀀스가 전부 초기화되므로, 통계도 전부 초기화(OnPlay/OnPause보다 강함)
		udpReceiver_.ResetMetrics();
		return true;
	}

	bool GameClient::Reset()
	{
		if (!IsConnected() || !SendHeaderOnly(Common::MessageType::Reset)) {
			return false;
		}

		udpReceiver_.ResetMetrics();
		return true;
	}

	bool GameClient::SetRate(Common::DataRate dataRate)
	{
		if (!IsConnected() || !SendSetRate(static_cast<uint32_t>(dataRate))) {
			return false;
		}

		udpReceiver_.SetExpectedDataRate(dataRate);
		return true;
	}

	bool GameClient::SendControlInput(float throttle, float yaw)
	{
		uint32_t myEntityId = 0;
		if (!IsConnected() || !entityWorld_.TryGetMyEntityId(myEntityId)) {
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
		payload.ElapsedSeconds = metrics.ElapsedSeconds;
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
