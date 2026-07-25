#pragma once

#include "HighResolutionTimer.h"
#include "Protocol.h"

#include <chrono>
#include <cstdint>
#include <thread>

namespace Common {

    // 시작 시점 기준으로 다음 틱을 계산해 누적 Sleep 오차를 줄임
    // 하나의 UDP 송신 워커 스레드에서만 사용
    class TimerCompensator {
        public:
            explicit TimerCompensator(DataRate dataRate)
                : dataRate_(dataRate)
            {
                Reset();
            }

            void SetDataRate(DataRate dataRate) {
                dataRate_ = dataRate;
                Reset();
            }

            void Reset() {
                startTime_ = HighResolutionTimer::Now();
                tickCount_ = 0;
            }

            void WaitForNextTick() {
                const HighResolutionTimer::TimePoint now = HighResolutionTimer::Now();

                HighResolutionTimer::TimePoint nextTick;
                do {
                    ++tickCount_;
                    nextTick = GetTickTime(tickCount_);
                } while(nextTick <= now);

                std::this_thread::sleep_until(nextTick);
            }

            DataRate GetDataRate() const {
                return dataRate_;
            }

        private:
            HighResolutionTimer::TimePoint GetTickTime(uint64_t tickCount) const
            {
                const long double seconds = static_cast<long double>(tickCount) / static_cast<uint32_t>(dataRate_);

                const std::chrono::duration<long double> elapsed(seconds);

                return startTime_ + std::chrono::duration_cast<HighResolutionTimer::Clock::duration>(elapsed);
            }

        private:
            DataRate dataRate_;
            HighResolutionTimer::TimePoint startTime_;
            uint64_t tickCount_ = 0;
    };

} // namespace Common