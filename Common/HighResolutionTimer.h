#pragma once

#include <chrono>
#include <cstdint>

namespace Common {

    // 시스템 시간 변경에 영향을 받지 않는 단조 증가 타이머
    class HighResolutionTimer {
        public:
            using Clock = std::chrono::steady_clock;
            using TimePoint = Clock::time_point;

            static TimePoint Now() {
                return Clock::now();
            }

            static uint64_t GetMicroseconds() {
                const auto elapsed = Now().time_since_epoch();

                return static_cast<uint64_t>(
                    std::chrono::duration_cast<std::chrono::microseconds>(elapsed).count()
                );
            }
    };

} // namespace Common