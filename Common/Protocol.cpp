#include "Protocol.h"

#include "BinarySerializer.h"
#include "ByteOrder.h"

#include <cstring>

namespace Common {

    void SerializeHeader(BinaryWriter& writer, MessageType type) {
        writer.WriteUInt8(static_cast<uint8_t>(type));
    }

    bool TryDeserializeHeader(BinaryReader& reader, MessageType& type) {
        uint8_t raw = 0;
        if (!reader.TryReadUInt8(raw)) {
            return false;
        }

        type = static_cast<MessageType>(raw);
        return true;
    }

    void SerializeRegisterUdpPort(BinaryWriter& writer, const RegisterUdpPortPayload& payload) {
        writer.WriteUInt16(payload.Port);
    }

    bool TryDeserializeRegisterUdpPort(BinaryReader& reader, RegisterUdpPortPayload& payload) {
        return reader.TryReadUInt16(payload.Port);
    }

    void SerializeSetRate(BinaryWriter& writer, const SetRatePayload& payload) {
        writer.WriteUInt32(payload.DataRateHz);
    }

    bool TryDeserializeSetRate(BinaryReader& reader, SetRatePayload& payload) {
        return reader.TryReadUInt32(payload.DataRateHz);
    }

    void SerializeEntityControlInput(BinaryWriter& writer, const EntityControlInputPayload& payload) {
        writer.WriteUInt32(payload.EntityId);
        writer.WriteUInt32(payload.SequenceId);
        writer.WriteFloat(payload.Throttle);
        writer.WriteFloat(payload.Yaw);
    }

    bool TryDeserializeEntityControlInput(BinaryReader& reader, EntityControlInputPayload& payload) {
        return reader.TryReadUInt32(payload.EntityId)
            && reader.TryReadUInt32(payload.SequenceId)
            && reader.TryReadFloat(payload.Throttle)
            && reader.TryReadFloat(payload.Yaw);
    }

    void SerializeEntityStateEntry(BinaryWriter& writer, const EntityStateEntry& entry) {
        writer.WriteUInt32(entry.EntityId);
        writer.WriteFloat(entry.PositionX);
        writer.WriteFloat(entry.PositionY);
        writer.WriteFloat(entry.Heading);
        writer.WriteFloat(entry.VelocityX);
        writer.WriteFloat(entry.VelocityY);
    }

    bool TryDeserializeEntityStateEntry(BinaryReader& reader, EntityStateEntry& entry) {
        return reader.TryReadUInt32(entry.EntityId)
            && reader.TryReadFloat(entry.PositionX)
            && reader.TryReadFloat(entry.PositionY)
            && reader.TryReadFloat(entry.Heading)
            && reader.TryReadFloat(entry.VelocityX)
            && reader.TryReadFloat(entry.VelocityY);
    }

    void SerializeEntityStateBatch(BinaryWriter& writer, const EntityStateBatchPayload& batch) {
        writer.WriteUInt64(batch.SequenceId);
        writer.WriteUInt64(batch.Timestamp);
        writer.WriteUInt16(static_cast<uint16_t>(batch.Entities.size()));

        for (const auto& entry : batch.Entities) {
            SerializeEntityStateEntry(writer, entry);
        }
    }

    bool TryDeserializeEntityStateBatch(BinaryReader& reader, EntityStateBatchPayload& batch) {
        uint16_t entityCount = 0;

        if (!reader.TryReadUInt64(batch.SequenceId) ||
            !reader.TryReadUInt64(batch.Timestamp) ||
            !reader.TryReadUInt16(entityCount))
        {
            return false;
        }

        batch.Entities.clear();
        batch.Entities.reserve(entityCount);

        for (uint16_t i = 0; i < entityCount; ++i) {
            EntityStateEntry entry;
            if (!TryDeserializeEntityStateEntry(reader, entry)) {
                return false;
            }

            batch.Entities.push_back(entry);
        }

        return true;
    }

    void SerializeEntitySpawn(BinaryWriter& writer, const EntitySpawnPayload& payload) {
        writer.WriteUInt32(payload.EntityId);
        writer.WriteUInt8(static_cast<uint8_t>(payload.Type));
        writer.WriteFloat(payload.PositionX);
        writer.WriteFloat(payload.PositionY);
        writer.WriteFloat(payload.Heading);
    }

    bool TryDeserializeEntitySpawn(BinaryReader& reader, EntitySpawnPayload& payload) {
        uint8_t rawType = 0;

        const bool ok = reader.TryReadUInt32(payload.EntityId)
            && reader.TryReadUInt8(rawType)
            && reader.TryReadFloat(payload.PositionX)
            && reader.TryReadFloat(payload.PositionY)
            && reader.TryReadFloat(payload.Heading);

        if (ok) {
            payload.Type = static_cast<EntityType>(rawType);
        }

        return ok;
    }

    void SerializeEntityDespawn(BinaryWriter& writer, const EntityDespawnPayload& payload) {
        writer.WriteUInt32(payload.EntityId);
    }

    bool TryDeserializeEntityDespawn(BinaryReader& reader, EntityDespawnPayload& payload) {
        return reader.TryReadUInt32(payload.EntityId);
    }

    void SerializeStop(BinaryWriter& writer, const StopPayload& payload) {
        writer.WriteDouble(payload.ElapsedSeconds);

        writer.WriteUInt64(payload.TotalReceivedCount);
        writer.WriteUInt64(payload.LossCount);
        writer.WriteUInt64(payload.OutOfOrderCount);
        writer.WriteDouble(payload.LossRate);

        writer.WriteUInt64(payload.IntervalSampleCount);
        writer.WriteDouble(payload.AverageReceiveIntervalMilliseconds);
        writer.WriteDouble(payload.MaxReceiveIntervalMilliseconds);
        writer.WriteDouble(payload.AverageIntervalDeviationMilliseconds);

        writer.WriteUInt64(payload.DelayedPacketCount);
        writer.WriteDouble(payload.DelayedPacketRate);

        writer.WriteUInt64(payload.LatencySampleCount);
        writer.WriteDouble(payload.AverageLatencyMilliseconds);
        writer.WriteDouble(payload.MinLatencyMilliseconds);
        writer.WriteDouble(payload.MaxLatencyMilliseconds);
    }

    bool TryDeserializeStop(BinaryReader& reader, StopPayload& payload) {
        return reader.TryReadDouble(payload.ElapsedSeconds)
            && reader.TryReadUInt64(payload.TotalReceivedCount)
            && reader.TryReadUInt64(payload.LossCount)
            && reader.TryReadUInt64(payload.OutOfOrderCount)
            && reader.TryReadDouble(payload.LossRate)
            && reader.TryReadUInt64(payload.IntervalSampleCount)
            && reader.TryReadDouble(payload.AverageReceiveIntervalMilliseconds)
            && reader.TryReadDouble(payload.MaxReceiveIntervalMilliseconds)
            && reader.TryReadDouble(payload.AverageIntervalDeviationMilliseconds)
            && reader.TryReadUInt64(payload.DelayedPacketCount)
            && reader.TryReadDouble(payload.DelayedPacketRate)
            && reader.TryReadUInt64(payload.LatencySampleCount)
            && reader.TryReadDouble(payload.AverageLatencyMilliseconds)
            && reader.TryReadDouble(payload.MinLatencyMilliseconds)
            && reader.TryReadDouble(payload.MaxLatencyMilliseconds);
    }

    void PatchEntityStateSequenceId(std::vector<uint8_t>& serializedPacket, uint64_t sequenceId) {
        // 레이아웃: MessageType 헤더(1바이트) 바로 다음이 SequenceId(8바이트, network order) -
        // SerializeEntityStateBatch가 SequenceId를 가장 먼저 쓰는 것과 짝이 맞아야 함
        constexpr size_t sequenceIdOffset = sizeof(uint8_t);

        if (serializedPacket.size() < sequenceIdOffset + sizeof(uint64_t)) {
            return;
        }

        const uint64_t networkValue = HostToNetwork64(sequenceId);
        std::memcpy(serializedPacket.data() + sequenceIdOffset, &networkValue, sizeof(networkValue));
    }

} // namespace Common
