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

		// 서버 Timestamp 기준 편도 지연 - 서버/클라이언트가 같은 컴퓨터에서 실행 중일 때만 유효
		uint64_t LatencySampleCount = 0;
		double AverageLatencyMilliseconds = 0.0;
		double MinLatencyMilliseconds = 0.0;
		double MaxLatencyMilliseconds = 0.0;
	};

	// UDP 시퀀스와 수신 간격을 분석
	class MetricsCollector {
	public:
		// sendTimestampMicroseconds: 서버가 Common::HighResolutionTimer로 찍어 보낸 송신 시각
		void OnPacketReceived(uint64_t sequenceId, uint64_t sendTimestampMicroseconds);

		// 전송률 변경 후 새 기준으로 간격을 계산
		void SetExpectedDataRate(Common::DataRate dataRate);

		// Pause, Stop, Play 이후 이전 수신 시각을 기준으로 삼지 않음
		void ResetReceiveTiming();

		void Reset();
		MetricsSnapshot GetSnapshot() const;

	private:
		void RecordReceiveInterval(std::chrono::steady_clock::time_point receivedTime, uint64_t sequenceId);
		double GetExpectedIntervalMilliseconds() const;

		// highestSequenceId_ 기준으로 너무 오래된 미수신 시퀀스는 확정 유실로 전환
		void ExpireStaleMissingSequenceIds(uint64_t highestSequenceId);

		// sendTimestampMicroseconds 대비 지금까지 걸린 시간을 지연으로 기록
		void RecordLatency(uint64_t sendTimestampMicroseconds);

	private:
		// 이 범위보다 오래된 미수신 시퀀스는 다시 안 올 것으로 보고 확정 유실 처리
		static constexpr uint64_t MissingSequenceWindow = 1000;

		uint64_t totalReceivedCount_ = 0;
		uint64_t outOfOrderCount_ = 0;
		uint64_t confirmedLostCount_ = 0;
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

		bool hasLatencySample_ = false;
		uint64_t latencySampleCount_ = 0;
		double totalLatencyMilliseconds_ = 0.0;
		double minLatencyMilliseconds_ = 0.0;
		double maxLatencyMilliseconds_ = 0.0;

		mutable std::mutex mutex_;
	};

} // namespace Client