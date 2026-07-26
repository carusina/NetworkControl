#include "ClientSession.h"

#include <iostream>

namespace Server {

	ClientSession::ClientSession(uint64_t sessionId, std::shared_ptr<Common::TcpSocket> controlSocket)
		: sessionId_(sessionId), controlSocket_(std::move(controlSocket)) { }

	void ClientSession::ProcessCommand(Common::CommandType command, uint32_t payload)
	{
		switch (command)
		{
			case Common::CommandType::Play:
				if (state_ == Common::SessionState::Stopped || state_ == Common::SessionState::Paused)
				{
					state_ = Common::SessionState::Playing;
					std::cout << "[Session " << sessionId_ << "] Playing" << std::endl;
				}
				break;

			case Common::CommandType::Pause:
				if (state_ == Common::SessionState::Playing)
				{
					state_ = Common::SessionState::Paused;
					std::cout << "[Session " << sessionId_ << "] Paused" << std::endl;
				}
				break;

			case Common::CommandType::Stop:
				if (state_ != Common::SessionState::Stopped)
				{
					state_ = Common::SessionState::Stopped;
					std::cout << "[Session " << sessionId_ << "] Stopped" << std::endl;
				}
				break;

			case Common::CommandType::Reset:
				state_ = Common::SessionState::Stopped;
				dataRate_ = Common::DataRate::Hz30;
				nextSequenceId_ = 0;

				std::cout << "[Session " << sessionId_ << "] Reset: Stopped, 30Hz, SequenceId 0" << std::endl;
				break;

			case Common::CommandType::SetRate:
				if (payload == static_cast<uint32_t>(Common::DataRate::Hz30))
				{
					dataRate_ = Common::DataRate::Hz30;
					std::cout << "[Session " << sessionId_ << "] Rate: 30Hz" << std::endl;
				}
				else if (payload == static_cast<uint32_t>(Common::DataRate::Hz60))
				{
					dataRate_ = Common::DataRate::Hz60;
					std::cout << "[Session " << sessionId_ << "] Rate: 60Hz" << std::endl;
				}
				else
				{
					std::cout << "[Session " << sessionId_ << "] Invalid rate: " << payload << std::endl;
				}
				break;
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

	Common::SessionState ClientSession::GetState() const {
		return state_;
	}

	Common::DataRate ClientSession::GetDataRate() const {
		return dataRate_;
	}

	uint64_t ClientSession::GetNextSequenceId() {
		return nextSequenceId_.fetch_add(1) + 1;
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

	void ClientSession::StopSession()
	{
		state_ = Common::SessionState::Stopped;

		// 여러 종료 경로에서 closesocket이 중복 호출되지 않도록 함
		if (!isControlConnectionClosed_.exchange(true) && controlSocket_) {
			controlSocket_->Close();
		}
	}

} // namespace Server