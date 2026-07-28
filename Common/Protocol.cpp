#include "Protocol.h"

#include "BinarySerializer.h"

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
        writer.WriteFloat(payload.Throttle);
        writer.WriteFloat(payload.Yaw);
    }

    bool TryDeserializeEntityControlInput(BinaryReader& reader, EntityControlInputPayload& payload) {
        return reader.TryReadFloat(payload.Throttle)
            && reader.TryReadFloat(payload.Yaw);
    }

    void SerializeEntityState(BinaryWriter& writer, const EntityStatePayload& payload) {
        writer.WriteUInt64(payload.SequenceId);
        writer.WriteUInt64(payload.Timestamp);
        writer.WriteUInt32(payload.EntityId);
        writer.WriteFloat(payload.PositionX);
        writer.WriteFloat(payload.PositionY);
        writer.WriteFloat(payload.Heading);
        writer.WriteFloat(payload.VelocityX);
        writer.WriteFloat(payload.VelocityY);
    }

    bool TryDeserializeEntityState(BinaryReader& reader, EntityStatePayload& payload) {
        return reader.TryReadUInt64(payload.SequenceId)
            && reader.TryReadUInt64(payload.Timestamp)
            && reader.TryReadUInt32(payload.EntityId)
            && reader.TryReadFloat(payload.PositionX)
            && reader.TryReadFloat(payload.PositionY)
            && reader.TryReadFloat(payload.Heading)
            && reader.TryReadFloat(payload.VelocityX)
            && reader.TryReadFloat(payload.VelocityY);
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

} // namespace Common
