#include "BinarySerializer.h"

#include "ByteOrder.h"

#include <cstring>

namespace Common {

    void BinaryWriter::WriteUInt8(uint8_t value) {
        buffer_.push_back(value);
    }

    void BinaryWriter::WriteUInt16(uint16_t value) {
        const uint16_t networkValue = HostToNetwork16(value);
        const uint8_t* bytes = reinterpret_cast<const uint8_t*>(&networkValue);
        buffer_.insert(buffer_.end(), bytes, bytes + sizeof(networkValue));
    }

    void BinaryWriter::WriteUInt32(uint32_t value) {
        const uint32_t networkValue = HostToNetwork32(value);
        const uint8_t* bytes = reinterpret_cast<const uint8_t*>(&networkValue);
        buffer_.insert(buffer_.end(), bytes, bytes + sizeof(networkValue));
    }

    void BinaryWriter::WriteUInt64(uint64_t value) {
        const uint64_t networkValue = HostToNetwork64(value);
        const uint8_t* bytes = reinterpret_cast<const uint8_t*>(&networkValue);
        buffer_.insert(buffer_.end(), bytes, bytes + sizeof(networkValue));
    }

    void BinaryWriter::WriteFloat(float value) {
        uint32_t bits = 0;
        std::memcpy(&bits, &value, sizeof(bits));
        WriteUInt32(bits);
    }

    void BinaryWriter::WriteDouble(double value) {
        uint64_t bits = 0;
        std::memcpy(&bits, &value, sizeof(bits));
        WriteUInt64(bits);
    }

    const std::vector<uint8_t>& BinaryWriter::Data() const {
        return buffer_;
    }

    BinaryReader::BinaryReader(const uint8_t* data, size_t size)
        : data_(data), size_(size) { }

    bool BinaryReader::HasRemaining(size_t count) const {
        return offset_ + count <= size_;
    }

    bool BinaryReader::TryReadUInt8(uint8_t& value) {
        if (!HasRemaining(sizeof(value))) {
            return false;
        }

        value = data_[offset_];
        offset_ += sizeof(value);
        return true;
    }

    bool BinaryReader::TryReadUInt16(uint16_t& value) {
        if (!HasRemaining(sizeof(value))) {
            return false;
        }

        uint16_t networkValue = 0;
        std::memcpy(&networkValue, data_ + offset_, sizeof(networkValue));
        value = NetworkToHost16(networkValue);
        offset_ += sizeof(networkValue);
        return true;
    }

    bool BinaryReader::TryReadUInt32(uint32_t& value) {
        if (!HasRemaining(sizeof(value))) {
            return false;
        }

        uint32_t networkValue = 0;
        std::memcpy(&networkValue, data_ + offset_, sizeof(networkValue));
        value = NetworkToHost32(networkValue);
        offset_ += sizeof(networkValue);
        return true;
    }

    bool BinaryReader::TryReadUInt64(uint64_t& value) {
        if (!HasRemaining(sizeof(value))) {
            return false;
        }

        uint64_t networkValue = 0;
        std::memcpy(&networkValue, data_ + offset_, sizeof(networkValue));
        value = NetworkToHost64(networkValue);
        offset_ += sizeof(networkValue);
        return true;
    }

    bool BinaryReader::TryReadFloat(float& value) {
        uint32_t bits = 0;
        if (!TryReadUInt32(bits)) {
            return false;
        }

        std::memcpy(&value, &bits, sizeof(value));
        return true;
    }

    bool BinaryReader::TryReadDouble(double& value) {
        uint64_t bits = 0;
        if (!TryReadUInt64(bits)) {
            return false;
        }

        std::memcpy(&value, &bits, sizeof(value));
        return true;
    }

} // namespace Common
