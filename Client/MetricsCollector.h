#pragma once

#include "../Common/Protocol.h"

#include <chrono>
#include <cstdint>
#include <mutex>
#include <unordered_set>

namespace Client {

	// UDP 수신 품질 통계
	struct MetricsSnapshot {
		uint64_t TotalReceivedCount = 0;
		uint64_t LossCount = 0;
		uint64_t OutOfOrderCount = 0;
		double LossRate = 0.0;

		uint64_t IntervalSampleCount = 0;
		double AverageReceiveIntervalMilliseconds = 0.0;
		double MaxReceiveIntervalMilliseconds = 0.0;
		double AverageIntervalDeviationMilliseconds = 0.0;

		uint64_t DelayedPacketCount = 0;
		double DelayedPacketRate = 0.0;
	};

	// UDP 시퀀스와 수신 간격을 분석
	class MetricsCollector {
	public:
		void OnPacketReceived(uint64_t sequenceId);

		// 전송률 변경 후 새 기준으로 간격을 계산
		void SetExpectedDataRate(Common::DataRate dataRate);

		// Pause, Stop, Play 이후 이전 수신 시각을 기준으로 삼지 않음
		void ResetReceiveTiming();

		void Reset();
		MetricsSnapshot GetSnapshot() const;

	private:
		void RecordReceiveInterval(std::chrono::steady_clock::time_point receivedTime, uint64_t sequenceId);
		double GetExpectedIntervalMilliseconds() const;

	private:
		uint64_t totalReceivedCount_ = 0;
		uint64_t outOfOrderCount_ = 0;
		uint64_t highestSequenceId_ = 0;
		bool hasReceivedPacket_ = false;

		std::unordered_set<uint64_t> missingSequenceIds_;
		
		Common::DataRate expectedDataRate_ = Common::DataRate::Hz30;

		bool hasTimingSample_ = false;
		uint64_t lastTimingSequenceId_ = 0;
		std::chrono::steady_clock::time_point lastReceiveTime_{};

		uint64_t intervalSampleCount_ = 0;
		double totalReceiveIntervalMilliseconds_ = 0.0;
		double maxReceiveIntervalMilliseconds_ = 0.0;
		double totalIntervalDeviationMilliseconds_ = 0.0;

		uint64_t delayedPacketCount_ = 0;

		mutable std::mutex mutex_;
	};

} // namespace Client