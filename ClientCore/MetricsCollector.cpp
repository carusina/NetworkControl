#include "MetricsCollector.h"

#include "../Common/HighResolutionTimer.h"

#include <cmath>

namespace Client {

	void MetricsCollector::OnPacketReceived(uint64_t sequenceId, uint64_t sendTimestampMicroseconds)
	{
		std::lock_guard<std::mutex> lock(mutex_);

		const auto receivedTime = std::chrono::steady_clock::now();

		++totalReceivedCount_;
		RecordLatency(sendTimestampMicroseconds);

		if (!hasReceivedPacket_)
		{
			highestSequenceId_ = sequenceId;
			hasReceivedPacket_ = true;
			statsStartTime_ = receivedTime;

			lastTimingSequenceId_ = sequenceId;
			lastReceiveTime_ = receivedTime;
			hasTimingSample_ = true;
			return;
		}

		if (sequenceId > highestSequenceId_)
		{
			for (uint64_t missingId = highestSequenceId_ + 1; missingId < sequenceId; ++missingId) {
				missingSequenceIds_.insert(missingId);
			}

			RecordReceiveInterval(receivedTime, sequenceId);
			highestSequenceId_ = sequenceId;

			ExpireStaleMissingSequenceIds(highestSequenceId_);
			return;
		}

		const auto missingIt = missingSequenceIds_.find(sequenceId);
		if (missingIt != missingSequenceIds_.end())
		{
			missingSequenceIds_.erase(missingIt);
			++outOfOrderCount_;
		}
	}

	void MetricsCollector::SetExpectedDataRate(Common::DataRate dataRate)
	{
		std::lock_guard<std::mutex> lock(mutex_);

		expectedDataRate_ = dataRate;
		hasTimingSample_ = false;
	}

	void MetricsCollector::OnPlay()
	{
		std::lock_guard<std::mutex> lock(mutex_);

		if (isPaused_)
		{
			accumulatedPausedSeconds_ += std::chrono::duration<double>(std::chrono::steady_clock::now() - pauseStartTime_).count();
			isPaused_ = false;
		}

		hasTimingSample_ = false;
	}

	void MetricsCollector::OnPause()
	{
		std::lock_guard<std::mutex> lock(mutex_);

		// 아직 첫 패킷도 못 받은 상태에서의 Pause는 경과 시간 계산에 영향이 없으므로 무시
		if (hasReceivedPacket_ && !isPaused_)
		{
			pauseStartTime_ = std::chrono::steady_clock::now();
			isPaused_ = true;
		}

		hasTimingSample_ = false;
	}

	void MetricsCollector::Reset()
	{
		std::lock_guard<std::mutex> lock(mutex_);

		totalReceivedCount_ = 0;
		outOfOrderCount_ = 0;
		confirmedLostCount_ = 0;
		highestSequenceId_ = 0;
		hasReceivedPacket_ = false;
		statsStartTime_ = {};
		isPaused_ = false;
		pauseStartTime_ = {};
		accumulatedPausedSeconds_ = 0.0;
		missingSequenceIds_.clear();

		// expectedDataRate_는 건드리지 않음 - 통계가 아니라 연결 설정(SetRate)이라, 서버의
		// dataRate_와 마찬가지로 Reset/Stop이 아니라 SetRate 호출로만 바뀌어야 함.
		// (예전엔 여기서 Hz30으로 강제로 되돌렸는데, 서버 쪽 dataRate_는 안 되돌리게 바뀌면서
		// "서버는 60Hz로 계속 보내는데 클라이언트는 30Hz를 기대"하는 불일치가 생겨 지연 판정이
		// 항상 통과하지 못하는 버그가 있었음)

		hasTimingSample_ = false;
		lastTimingSequenceId_ = 0;
		lastReceiveTime_ = {};

		intervalSampleCount_ = 0;
		totalReceiveIntervalMilliseconds_ = 0.0;
		maxReceiveIntervalMilliseconds_ = 0.0;
		totalIntervalDeviationMilliseconds_ = 0.0;

		delayedPacketCount_ = 0;

		hasLatencySample_ = false;
		latencySampleCount_ = 0;
		totalLatencyMilliseconds_ = 0.0;
		minLatencyMilliseconds_ = 0.0;
		maxLatencyMilliseconds_ = 0.0;
	}

	MetricsSnapshot MetricsCollector::GetSnapshot() const
	{
		std::lock_guard<std::mutex> lock(mutex_);

		MetricsSnapshot snapshot;

		if (hasReceivedPacket_)
		{
			const auto now = std::chrono::steady_clock::now();

			double pausedSeconds = accumulatedPausedSeconds_;
			if (isPaused_) {
				pausedSeconds += std::chrono::duration<double>(now - pauseStartTime_).count();
			}

			snapshot.ElapsedSeconds = std::chrono::duration<double>(now - statsStartTime_).count() - pausedSeconds;
		}

		snapshot.TotalReceivedCount = totalReceivedCount_;
		snapshot.LossCount = confirmedLostCount_ + static_cast<uint64_t>(missingSequenceIds_.size());
		snapshot.OutOfOrderCount = outOfOrderCount_;

		const uint64_t expectedPacketCount = snapshot.TotalReceivedCount + snapshot.LossCount;

		if (expectedPacketCount != 0) {
			snapshot.LossRate = static_cast<double>(snapshot.LossCount) / static_cast<double>(expectedPacketCount) * 100.0;
		}

		snapshot.IntervalSampleCount = intervalSampleCount_;

		if (intervalSampleCount_ != 0)
		{
			snapshot.AverageReceiveIntervalMilliseconds = totalReceiveIntervalMilliseconds_ / static_cast<double>(intervalSampleCount_);

			snapshot.MaxReceiveIntervalMilliseconds = maxReceiveIntervalMilliseconds_;

			snapshot.AverageIntervalDeviationMilliseconds = totalIntervalDeviationMilliseconds_ / static_cast<double>(intervalSampleCount_);

			snapshot.DelayedPacketRate = static_cast<double>(delayedPacketCount_) / static_cast<double>(intervalSampleCount_) * 100.0;
		}

		snapshot.DelayedPacketCount = delayedPacketCount_;

		snapshot.LatencySampleCount = latencySampleCount_;

		if (latencySampleCount_ != 0)
		{
			snapshot.AverageLatencyMilliseconds = totalLatencyMilliseconds_ / static_cast<double>(latencySampleCount_);
			snapshot.MinLatencyMilliseconds = minLatencyMilliseconds_;
			snapshot.MaxLatencyMilliseconds = maxLatencyMilliseconds_;
		}

		return snapshot;
	}
	
	void MetricsCollector::RecordReceiveInterval(std::chrono::steady_clock::time_point receivedTime, uint64_t sequenceId)
	{
		if (!hasTimingSample_)
		{
			lastTimingSequenceId_ = sequenceId;
			lastReceiveTime_ = receivedTime;
			hasTimingSample_ = true;
			return;
		}

		const uint64_t sequenceDifference = sequenceId - lastTimingSequenceId_;
		if (sequenceDifference == 0) {
			return;
		}

		const std::chrono::duration<double, std::milli> elapsed = receivedTime - lastReceiveTime_;

		const double actualElapsedMilliseconds = elapsed.count();
		const double expectedIntervalMilliseconds = GetExpectedIntervalMilliseconds();

		// 유실된 패킷 수만큼 기대 간격을 증가
		const double expectedElapsedMilliseconds = expectedIntervalMilliseconds * static_cast<double>(sequenceDifference);

		// 패킷 하나 기준 수신 간격
		const double normalizedIntervalMilliseconds = actualElapsedMilliseconds / static_cast<double>(sequenceDifference);

		const double deviationMilliseconds = std::fabs(normalizedIntervalMilliseconds - expectedIntervalMilliseconds);
		
		++intervalSampleCount_;

		totalReceiveIntervalMilliseconds_ += normalizedIntervalMilliseconds;

		if (normalizedIntervalMilliseconds > maxReceiveIntervalMilliseconds_) {
			maxReceiveIntervalMilliseconds_ = normalizedIntervalMilliseconds;
		}

		totalIntervalDeviationMilliseconds_ += deviationMilliseconds;

		// 실제 경과 시간이 예상 시간의 1.5배보다 크면 지연으로 판단
		if (actualElapsedMilliseconds > expectedElapsedMilliseconds * 1.5) {
			++delayedPacketCount_;
		}

		lastTimingSequenceId_ = sequenceId;
		lastReceiveTime_ = receivedTime;
	}

	double MetricsCollector::GetExpectedIntervalMilliseconds() const {
		return 1000.0 / static_cast<double>(static_cast<uint32_t>(expectedDataRate_));
	}

	void MetricsCollector::RecordLatency(uint64_t sendTimestampMicroseconds)
	{
		const uint64_t nowMicroseconds = Common::HighResolutionTimer::GetMicroseconds();

		// steady_clock의 0점은 "이 컴퓨터가 부팅된 시점"이라, 서버와 클라이언트가 서로 다른
		// 컴퓨터에서 실행 중이면 두 시계 기준이 달라 뺄셈 결과가 의미 없어짐(음수 포함).
		// 같은 컴퓨터에서 실행 중일 때만 유효한 값으로 취급.
		if (nowMicroseconds < sendTimestampMicroseconds) {
			return;
		}

		const double latencyMilliseconds = static_cast<double>(nowMicroseconds - sendTimestampMicroseconds) / 1000.0;

		++latencySampleCount_;
		totalLatencyMilliseconds_ += latencyMilliseconds;

		if (!hasLatencySample_ || latencyMilliseconds < minLatencyMilliseconds_) {
			minLatencyMilliseconds_ = latencyMilliseconds;
		}

		if (!hasLatencySample_ || latencyMilliseconds > maxLatencyMilliseconds_) {
			maxLatencyMilliseconds_ = latencyMilliseconds;
		}

		hasLatencySample_ = true;
	}

	void MetricsCollector::ExpireStaleMissingSequenceIds(uint64_t highestSequenceId)
	{
		if (highestSequenceId <= MissingSequenceWindow) {
			return;
		}

		const uint64_t expiryThreshold = highestSequenceId - MissingSequenceWindow;

		for (auto it = missingSequenceIds_.begin(); it != missingSequenceIds_.end(); )
		{
			if (*it <= expiryThreshold)
			{
				it = missingSequenceIds_.erase(it);
				++confirmedLostCount_;
			}
			else
			{
				++it;
			}
		}
	}

} // namespace Client