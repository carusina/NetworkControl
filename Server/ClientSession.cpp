#include "ClientSession.h"

#include <cmath>
#include <iostream>

namespace Server {

	namespace {

		// 물리 모델 상수 - 시작점으로 잡은 값, 나중에 튜닝 가능
		constexpr float YawRateMaxRadiansPerSecond = 2.0f;
		constexpr float AccelerationUnitsPerSecondSquared = 20.0f;
		constexpr float MaxSpeedUnitsPerSecond = 50.0f;

		// 클수록 빨리 감속 (1.0 ≈ throttle 0일 때 정지까지 걸리는 시간상수가 대략 1초)
		constexpr float DragPerSecond = 1.0f;

		// std::clamp(C++17) 대신 - 이 프로젝트가 그보다 이전 표준으로 컴파일됨
		float Clamp(float value, float minValue, float maxValue) {
			if (value < minValue) return minValue;
			if (value > maxValue) return maxValue;
			return value;
		}

	} // namespace

	ClientSession::ClientSession(uint64_t sessionId, std::shared_ptr<Common::TcpSocket> controlSocket)
		: sessionId_(sessionId), controlSocket_(std::move(controlSocket)) { }

	void ClientSession::Play()
	{
		if (state_ == Common::SessionState::Stopped || state_ == Common::SessionState::Paused)
		{
			state_ = Common::SessionState::Playing;
			std::cout << "[Session " << sessionId_ << "] Playing" << std::endl;
		}
	}

	void ClientSession::Pause()
	{
		if (state_ == Common::SessionState::Playing)
		{
			state_ = Common::SessionState::Paused;
			std::cout << "[Session " << sessionId_ << "] Paused" << std::endl;
		}
	}

	// Stop: 통계(수신 시퀀스 번호 체계)와 엔티티 상태(위치 등) 둘 다 초기화
	void ClientSession::Stop()
	{
		state_ = Common::SessionState::Stopped;
		nextSequenceId_ = 0;

		{
			std::lock_guard<std::mutex> lock(entityMutex_);
			positionX_ = 0.0f;
			positionY_ = 0.0f;
			heading_ = 0.0f;
			speed_ = 0.0f;
			velocityX_ = 0.0f;
			velocityY_ = 0.0f;
			throttle_ = 0.0f;
			yaw_ = 0.0f;
		}

		std::cout << "[Session " << sessionId_ << "] Stopped: SequenceId 0, entity origin" << std::endl;
	}

	// Reset: 통계(수신 시퀀스 번호 체계)만 초기화 - 엔티티 위치/속도는 그대로 둠
	void ClientSession::Reset()
	{
		nextSequenceId_ = 0;
		std::cout << "[Session " << sessionId_ << "] Reset: SequenceId 0 (entity state kept)" << std::endl;
	}

	void ClientSession::SetRate(uint32_t dataRateHz)
	{
		if (dataRateHz == static_cast<uint32_t>(Common::DataRate::Hz30))
		{
			dataRate_ = Common::DataRate::Hz30;
			std::cout << "[Session " << sessionId_ << "] Rate: 30Hz" << std::endl;
		}
		else if (dataRateHz == static_cast<uint32_t>(Common::DataRate::Hz60))
		{
			dataRate_ = Common::DataRate::Hz60;
			std::cout << "[Session " << sessionId_ << "] Rate: 60Hz" << std::endl;
		}
		else
		{
			std::cout << "[Session " << sessionId_ << "] Invalid rate: " << dataRateHz << std::endl;
		}
	}

	void ClientSession::RegisterUdpEndpoint(const std::string& ip, uint16_t port)
	{
		std::lock_guard<std::mutex> lock(endpointMutex_);

		udpIpAddress_ = ip;
		udpPort_ = port;

		std::cout << "[Session " << sessionId_ << "] UDP endpoint:" << udpIpAddress_ << ":" << udpPort_ << std::endl;
	}

	uint64_t ClientSession::GetSessionId() const {
		return sessionId_;
	}

	uint32_t ClientSession::GetEntityId() const {
		return static_cast<uint32_t>(sessionId_);
	}

	Common::SessionState ClientSession::GetState() const {
		return state_;
	}

	Common::DataRate ClientSession::GetDataRate() const {
		return dataRate_;
	}

	uint64_t ClientSession::GetNextSequenceId() {
		return nextSequenceId_.fetch_add(1) + 1;
	}

	uint64_t ClientSession::GetSentPacketCount() const {
		return nextSequenceId_;
	}

	bool ClientSession::TryGetUdpEndpoint(std::string& ip, uint16_t& port) const
	{
		std::lock_guard<std::mutex> lock(endpointMutex_);

		if (udpIpAddress_.empty() || udpPort_ == 0) {
			return false;
		}

		ip = udpIpAddress_;
		port = udpPort_;
		return true;
	}

	Common::TcpSocket& ClientSession::GetControlSocket() {
		return *controlSocket_;
	}

	void ClientSession::ApplyControlInput(uint32_t sequenceId, float throttle, float yaw)
	{
		std::lock_guard<std::mutex> lock(entityMutex_);

		// UDP 역전으로 오래된 입력이 뒤늦게 도착한 경우 - 이미 적용된 최신 입력을 덮어쓰지 않도록 무시
		if (sequenceId <= lastAppliedControlInputSequenceId_) {
			return;
		}

		lastAppliedControlInputSequenceId_ = sequenceId;
		throttle_ = Clamp(throttle, -1.0f, 1.0f);
		yaw_ = Clamp(yaw, -1.0f, 1.0f);
	}

	void ClientSession::StepPhysics(double deltaSeconds)
	{
		std::lock_guard<std::mutex> lock(entityMutex_);

		const float dt = static_cast<float>(deltaSeconds);

		heading_ += yaw_ * YawRateMaxRadiansPerSecond * dt;

		speed_ += throttle_ * AccelerationUnitsPerSecondSquared * dt;

		// 감속 - throttle을 0으로 줘도 관성으로 계속 미끄러지지 않고 서서히 멈추게 함
		speed_ *= Clamp(1.0f - DragPerSecond * dt, 0.0f, 1.0f);

		speed_ = Clamp(speed_, -MaxSpeedUnitsPerSecond, MaxSpeedUnitsPerSecond);

		velocityX_ = std::cos(heading_) * speed_;
		velocityY_ = std::sin(heading_) * speed_;

		positionX_ += velocityX_ * dt;
		positionY_ += velocityY_ * dt;
	}

	Common::EntityStateEntry ClientSession::BuildEntityStateEntry() const
	{
		std::lock_guard<std::mutex> lock(entityMutex_);

		Common::EntityStateEntry entry;
		entry.EntityId = GetEntityId();
		entry.PositionX = positionX_;
		entry.PositionY = positionY_;
		entry.Heading = heading_;
		entry.VelocityX = velocityX_;
		entry.VelocityY = velocityY_;

		return entry;
	}

	Common::EntitySpawnPayload ClientSession::BuildEntitySpawnPayload() const
	{
		std::lock_guard<std::mutex> lock(entityMutex_);

		Common::EntitySpawnPayload payload;
		payload.EntityId = GetEntityId();
		payload.Type = Common::EntityType::PlayerHelicopter;
		payload.PositionX = positionX_;
		payload.PositionY = positionY_;
		payload.Heading = heading_;

		return payload;
	}

	void ClientSession::StopSession()
	{
		state_ = Common::SessionState::Stopped;

		// 여러 종료 경로에서 closesocket이 중복 호출되지 않도록 함
		if (!isControlConnectionClosed_.exchange(true) && controlSocket_) {
			controlSocket_->Close();
		}
	}

} // namespace Server
