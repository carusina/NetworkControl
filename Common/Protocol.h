#pragma once

#include <cstdint>

namespace Common {

    // 네트워크 전송 레이아웃 고정
    #pragma pack(push, 1)

    // UDP 데이터 채널에서 전송하는 패킷
    struct Packet {
        uint64_t SequenceId; // 세션별 패킷 순번
        uint64_t Timestamp; // 서버 기준 생성 시각
    };

    // TCP 제어 채널에서 사용하는 명령
    enum class CommandType : uint8_t {
        RegisterUdpPort, // UDP 수신 포트 등록
        Play,            // 스트리밍 시작 또는 재개
        Pause,           // 스트리밍 일시 정지
        Stop,            // 스트리밍 중지
        Reset,           // 세션 상태 초기화
        SetRate          // 송신 주기 변경
    };

    // TCP  제어 메시지의 고정 형식
    struct ControlMessage {
        CommandType Type;
        uint32_t Payload; // UDP 포트 번호 또는 DataRate 값
    };
    #pragma pack(pop)

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

} // namespace Common