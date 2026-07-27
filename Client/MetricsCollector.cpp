#include "MetricsCollector.h"

#include <cmath>

namespace Client {

	void MetricsCollector::OnPacketReceived(uint64_t sequenceId)
	{
		std::lock_guard<std::mutex> lock(mutex_);

		const auto receivedTime = std::chrono::steady_clock::now();
		
		++totalReceivedCount_;

		if (!hasReceivedPacket_)
		{
			highestSequenceId_ = sequenceId;
			hasReceivedPacket_ = true;

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

	void MetricsCollector::ResetReceiveTiming()
	{
		std::lock_guard<std::mutex> lock(mutex_);
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
		missingSequenceIds_.clear();

		expectedDataRate_ = Common::DataRate::Hz30;

		hasTimingSample_ = false;
		lastTimingSequenceId_ = 0;
		lastReceiveTime_ = {};

		intervalSampleCount_ = 0;
		totalReceiveIntervalMilliseconds_ = 0.0;
		maxReceiveIntervalMilliseconds_ = 0.0;
		totalIntervalDeviationMilliseconds_ = 0.0;

		delayedPacketCount_ = 0;
	}

	MetricsSnapshot MetricsCollector::GetSnapshot() const
	{
		std::lock_guard<std::mutex> lock(mutex_);

		MetricsSnapshot snapshot;

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