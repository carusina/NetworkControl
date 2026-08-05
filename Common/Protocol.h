#pragma once

#include <cstdint>
#include <vector>

namespace Common {

    class BinaryWriter;
    class BinaryReader;

    // 모든 메시지 맨 앞에 붙는 1바이트 타입 태그로 Payload 모양을 결정
    enum class MessageType : uint8_t {
        RegisterUdpPort,      // UDP 수신 포트 등록 (TCP)
        Play,                 // 스트리밍 시작 또는 재개 (TCP)
        Pause,                // 스트리밍 일시 정지 (TCP)
        Stop,                 // 스트리밍 중지 (TCP)
        Reset,                // 세션/엔티티 상태 초기화 (TCP)
        SetRate,              // 수신 주기 변경 (TCP)
        EntityControlInput,   // 조종 입력: Throttle/Yaw (UDP, client -> server)
        EntityState,          // 엔티티 상태 브로드캐스트 (UDP, server -> client)
        EntitySpawn,          // 엔티티 생성 알림 (TCP, server -> client)
        EntityDespawn         // 엔티티 제거 알림 (TCP, server -> client)
    };

    // 엔티티 종류 - 지금은 플레이어 헬기 하나뿐이라도 확장 대비로 태그를 둠
    enum class EntityType : uint8_t {
        PlayerHelicopter
    };

    // UDP 데이터 전송 빈도
    enum class DataRate : uint32_t {
        Hz30 = 30,
        Hz60 = 60
    };

    // 클라이언트 세션의 스트리밍 상태
    enum class SessionState : uint8_t {
        Stopped,
        Playing,
        Paused
    };

    struct RegisterUdpPortPayload {
        uint16_t Port = 0;
    };

    struct SetRatePayload {
        uint32_t DataRateHz = 0;
    };

    // Throttle/Yaw 둘 다 -1.0 ~ 1.0 범위
    // EntityId를 실어 보내서 서버가 발신 주소로 세션을 매번 스캔하지 않고 바로 조회할 수 있게 함
    // SequenceId는 클라이언트가 보낼 때마다 증가시키는 값 - UDP 역전으로 오래된 입력이
    // 늦게 도착해 최신 입력을 덮어쓰지 않도록 서버가 이 값으로 최신 여부를 판단
    struct EntityControlInputPayload {
        uint32_t EntityId = 0;
        uint32_t SequenceId = 0;
        float Throttle = 0.0f;
        float Yaw = 0.0f;
    };

    // 엔티티 하나의 상태 - 배치 안에서 고정 크기로 이어 붙여 보냄
    struct EntityStateEntry {
        uint32_t EntityId = 0;
        float PositionX = 0.0f;
        float PositionY = 0.0f;
        float Heading = 0.0f;
        float VelocityX = 0.0f;
        float VelocityY = 0.0f;
    };

    // 한 틱에 한 수신자에게 보내는 모든 엔티티 상태를 묶은 패킷.
    // SequenceId/Timestamp는 패킷(수신자+틱) 단위로 한 번만 있음 - 엔티티마다 반복해서
    // 넣으면 패킷 하나 유실될 때 마치 엔티티 수만큼 유실된 것처럼 통계가 부풀려지기 때문
    struct EntityStateBatchPayload {
        uint64_t SequenceId = 0;   // 수신자별 UDP 패킷 순번 (유실/역전 통계용)
        uint64_t Timestamp = 0;    // 서버 기준 생성 시각 (지연 측정용)
        std::vector<EntityStateEntry> Entities;
    };

    struct EntitySpawnPayload {
        uint32_t EntityId = 0;
        EntityType Type = EntityType::PlayerHelicopter;
        float PositionX = 0.0f;
        float PositionY = 0.0f;
        float Heading = 0.0f;
    };

    struct EntityDespawnPayload {
        uint32_t EntityId = 0;
    };

    // Stop 시점에 클라이언트가 보고하는 마지막 수신 통계 - MetricsSnapshot 계산은 클라이언트만
    // 할 수 있어서(서버는 클라이언트가 실제로 뭘 받았는지 알 방법이 없음), 서버 콘솔에서 확인하려면
    // 클라이언트가 이 값을 Stop 메시지에 실어 보내야 함. Client::MetricsSnapshot과 필드가 1:1로 대응
    struct StopPayload {
        double ElapsedSeconds = 0.0;

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

        uint64_t LatencySampleCount = 0;
        double AverageLatencyMilliseconds = 0.0;
        double MinLatencyMilliseconds = 0.0;
        double MaxLatencyMilliseconds = 0.0;
    };

    // Play/Pause/Reset은 헤더(MessageType)만 있고 별도 Payload가 없음. Stop은 StopPayload를 실어 보냄

    // 각 Payload의 직렬화된 바이트 수 - TCP/UDP 수신 측에서 헤더 다음 몇 바이트를 읽어야 하는지 알려줌
    // (struct의 sizeof는 컴파일러 패딩이 낄 수 있어 실제 전송 크기와 다를 수 있으므로 쓰지 않음)
    constexpr size_t RegisterUdpPortPayloadSize = sizeof(uint16_t);
    constexpr size_t SetRatePayloadSize = sizeof(uint32_t);
    constexpr size_t EntityControlInputPayloadSize = sizeof(uint32_t) * 2 + sizeof(float) * 2;
    constexpr size_t EntityStateEntrySize = sizeof(uint32_t) + sizeof(float) * 5;
    constexpr size_t EntitySpawnPayloadSize = sizeof(uint32_t) + sizeof(uint8_t) + sizeof(float) * 3;
    constexpr size_t EntityDespawnPayloadSize = sizeof(uint32_t);
    constexpr size_t StopPayloadSize = sizeof(uint64_t) * 15;

    void SerializeHeader(BinaryWriter& writer, MessageType type);
    bool TryDeserializeHeader(BinaryReader& reader, MessageType& type);

    void SerializeRegisterUdpPort(BinaryWriter& writer, const RegisterUdpPortPayload& payload);
    bool TryDeserializeRegisterUdpPort(BinaryReader& reader, RegisterUdpPortPayload& payload);

    void SerializeSetRate(BinaryWriter& writer, const SetRatePayload& payload);
    bool TryDeserializeSetRate(BinaryReader& reader, SetRatePayload& payload);

    void SerializeEntityControlInput(BinaryWriter& writer, const EntityControlInputPayload& payload);
    bool TryDeserializeEntityControlInput(BinaryReader& reader, EntityControlInputPayload& payload);

    void SerializeEntityStateEntry(BinaryWriter& writer, const EntityStateEntry& entry);
    bool TryDeserializeEntityStateEntry(BinaryReader& reader, EntityStateEntry& entry);

    // 엔티티 개수(uint16) + SerializeEntityStateEntry를 그만큼 이어 붙임
    void SerializeEntityStateBatch(BinaryWriter& writer, const EntityStateBatchPayload& batch);
    bool TryDeserializeEntityStateBatch(BinaryReader& reader, EntityStateBatchPayload& batch);

    void SerializeEntitySpawn(BinaryWriter& writer, const EntitySpawnPayload& payload);
    bool TryDeserializeEntitySpawn(BinaryReader& reader, EntitySpawnPayload& payload);

    void SerializeEntityDespawn(BinaryWriter& writer, const EntityDespawnPayload& payload);
    bool TryDeserializeEntityDespawn(BinaryReader& reader, EntityDespawnPayload& payload);

    void SerializeStop(BinaryWriter& writer, const StopPayload& payload);
    bool TryDeserializeStop(BinaryReader& reader, StopPayload& payload);

    // EntityState 배치에서 SequenceId만 자리에서 덮어씀 - 같은 틱의 모든 수신자는 Timestamp/Entities가
    // 동일하므로 SerializeEntityStateBatch로 한 번만 직렬화해두고, 수신자마다 다른 SequenceId만 이
    // 함수로 patch하면 수신자 수만큼 매번 전체를 다시 직렬화하는 비용(수신자 수 x 엔티티 수)을 피할 수 있음.
    // serializedPacket은 SerializeHeader(EntityState) 다음에 SerializeEntityStateBatch를 호출한 결과여야 함
    void PatchEntityStateSequenceId(std::vector<uint8_t>& serializedPacket, uint64_t sequenceId);

} // namespace Common
