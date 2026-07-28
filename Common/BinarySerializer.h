#pragma once

#include <cstdint>
#include <cstddef>
#include <vector>

namespace Common {

    // 메시지 페이로드를 네트워크 바이트오더 바이트 스트림으로 직렬화
    class BinaryWriter {
        public:
            void WriteUInt8(uint8_t value);
            void WriteUInt16(uint16_t value);
            void WriteUInt32(uint32_t value);
            void WriteUInt64(uint64_t value);
            void WriteFloat(float value);

            const std::vector<uint8_t>& Data() const;

        private:
            std::vector<uint8_t> buffer_;
    };

    // 바이트 스트림에서 순서대로 읽어 호스트 바이트오더 값으로 복원
    // 데이터가 부족하면 false를 반환하고 커서를 되돌리지 않음(호출부에서 실패로 처리)
    class BinaryReader {
        public:
            BinaryReader(const uint8_t* data, size_t size);

            bool TryReadUInt8(uint8_t& value);
            bool TryReadUInt16(uint16_t& value);
            bool TryReadUInt32(uint32_t& value);
            bool TryReadUInt64(uint64_t& value);
            bool TryReadFloat(float& value);

        private:
            bool HasRemaining(size_t count) const;

        private:
            const uint8_t* data_;
            size_t size_;
            size_t offset_ = 0;
    };

} // namespace Common
