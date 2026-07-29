# NetworkControl

TCP로 제어하고 UDP로 고주기 데이터를 주고받는 서버/클라이언트 네트워크 시스템. **2D 멀티플레이어 헬기 시뮬레이션**(서버 권위 물리)을 구현했고, 지금은 Client 네트워킹 로직을 재사용 가능한 라이브러리(`ClientCore`)로 뽑아낸 상태 — 다음은 이 위에 C++/CLI Wrapper(.dll) → C# WPF GUI를 얹는 단계.

## 기술 스택 / 빌드 환경

- Windows, Visual Studio 2022 (v143 toolset), C++17 이전 표준(`std::clamp` 등 C++17 전용 stdlib 함수는 못 씀 — 프로젝트 표준을 올리지 않고 직접 만든 헬퍼로 대체하는 관례)
- 순수 Winsock2 기반, 외부 라이브러리 의존성 없음 (JSON/vcpkg 패키지 미사용)
- 솔루션 2개: `Server/Server.sln`, `Client/Client.sln`(+ `ClientCore.vcxproj`를 프로젝트 참조로 포함). `Common/` 소스는 각 최상위 프로젝트(`Server`, `ClientCore`)가 직접 컴파일 목록에 포함해서 링크한다 — **Common에 파일을 추가하면 `Server.vcxproj`와 `ClientCore.vcxproj` 양쪽에 수동으로 `ClCompile`/`ClInclude` 항목을 추가해야 한다** (자동으로 안 잡힘, 이 프로젝트에서 반복적으로 겪은 이슈).

## 프로젝트 구조

```
Common/       서버·클라이언트가 공유하는 프로토콜, 소켓 RAII 래퍼, 타이머, 설정 로더
Server/       TCP 제어 + UDP 물리 시뮬레이션/브로드캐스트 콘솔 앱
ClientCore/   클라이언트 네트워킹 로직 (정적 라이브러리) - 콘솔/GUI가 공유
Client/       ClientCore를 쓰는 콘솔 REPL (GameClient 하나 + 명령 파싱/출력)
```

| 위치 | 파일 | 역할 |
|---|---|---|
| Common | `Protocol.h` / `.cpp` | 와이어 프로토콜: `MessageType`, 메시지별 Payload 구조체, Serialize/Deserialize 함수 |
| Common | `BinarySerializer.h` / `.cpp` | `BinaryWriter`/`BinaryReader` — 네트워크 바이트오더로 직렬화/역직렬화하는 얇은 유틸 |
| Common | `ByteOrder.h` | 16/32/64비트 호스트↔네트워크 바이트오더 변환 |
| Common | `Ipv4Endpoint.h` / `.cpp` | IP+포트 값 타입 (`sockaddr_in` 래핑) |
| Common | `TcpSocket.h` / `.cpp`, `UdpSocket.h` / `.cpp` | Winsock 소켓 RAII 래퍼 (move-only, 소멸자에서 자동 close) |
| Common | `SocketRuntime.h` / `.cpp` | `WSAStartup`/`WSACleanup` RAII |
| Common | `HighResolutionTimer.h`, `TimerCompensator.h` | 단조 시계, 누적 오차 없는 주기 대기 |
| Common | `Config.h` / `.cpp` | `ServerConfig`/`ClientConfig` + ini 파일 로더 |
| Server | `Main.cpp` | 진입점 — 서비스 3개 조립 |
| Server | `SessionManager.h` / `.cpp` | 접속 세션 저장소 (조회 병렬, 추가/삭제 배타적) |
| Server | `ClientSession.h` / `.cpp` | 세션 1명의 제어 상태 + 엔티티(헬기) 시뮬레이션 상태 |
| Server | `TcpControlServer.h` / `.cpp` | TCP 연결 수락, 명령 처리, Spawn/Despawn 브로드캐스트 |
| Server | `UdpStreamingService.h` / `.cpp` | 60Hz 틱: 물리 계산 + EntityState 브로드캐스트 |
| Server | `UdpControlInputReceiver.h` / `.cpp` | 클라이언트의 조종 입력(UDP) 수신 |
| ClientCore | `GameClient.h` / `.cpp` | 콘솔/GUI 공용 파사드 — TCP 접속+핸드셰이크, 명령 전송, 엔티티/통계 조회를 캡슐화 (콘솔 I/O 없음) |
| ClientCore | `UdpReceiver.h` / `.cpp` | EntityState 수신 + 조종 입력 송신 |
| ClientCore | `MetricsCollector.h` / `.cpp` | 수신 품질 통계 (유실/역전/간격/지연) |
| ClientCore | `EntityWorld.h` / `.cpp` | 서버가 알려준 엔티티들의 최신 상태 맵 |
| ClientCore | `TcpMessageReceiver.h` / `.cpp` | TCP로 오는 Spawn/Despawn을 받는 백그라운드 스레드 |
| Client | `Main.cpp` | 진입점 + 대화형 REPL (`GameClient` 하나 생성 + 명령 파싱/화면 출력만 담당) |

## 아키텍처 개요

```
                    TCP (제어 채널, 신뢰성 필요)
   Client  ────────────────────────────────────────►  Server
           RegisterUdpPort / Play / Pause / Stop /
           Reset / SetRate                              │
           ◄──────────────────────────────────────────  │
             EntitySpawn / EntityDespawn (브로드캐스트)   │
                                                         │
                    UDP (데이터 채널, 유실 허용)             │
   Client  ────────────────────────────────────────►  Server
             EntityControlInput (Throttle/Yaw)           │
           ◄──────────────────────────────────────────  │
             EntityState (60Hz 틱, 모든 Playing 엔티티)
```

- **TCP**: 접속마다 서버가 스레드 하나(`ControlWorker`, detach)를 붙여 명령을 순서대로 처리. 신뢰성이 중요한 접속 등록/재생 제어/엔티티 생성·삭제 알림에 사용.
- **UDP**: 클라이언트→서버는 조종 입력(`EntityControlInput`), 서버→클라이언트는 엔티티 상태(`EntityState`). 유실돼도 다음 틱이 금방 오므로 허용.
- **서버 권위(authoritative) 시뮬레이션**: 클라이언트는 위치를 직접 계산하지 않고 Throttle/Yaw만 보낸다. 서버가 `ClientSession::StepPhysics()`로 실제 위치/속도/방향을 계산해서 모든 클라이언트에 뿌린다.
- **멀티플레이 모델**: 접속 클라이언트 1명 = 헬기 엔티티 1개. `EntityId`는 `ClientSession`의 `SessionId`를 그대로 재사용한다.

## 와이어 프로토콜

모든 메시지는 **1바이트 헤더(`MessageType`) + 메시지별 고정 크기 Payload**로 이루어진다. `#pragma pack` 구조체를 그대로 `send`하는 대신, `BinaryWriter`/`BinaryReader`로 필드 하나씩 직접 쓰고 읽는다 (컴파일러 패딩에 영향받지 않고, 메시지 타입마다 다른 모양의 Payload를 표현하기 위함).

```cpp
enum class MessageType : uint8_t {
    RegisterUdpPort, Play, Pause, Stop, Reset, SetRate,   // TCP, 클라이언트 -> 서버
    EntityControlInput,                                    // UDP, 클라이언트 -> 서버
    EntityState,                                           // UDP, 서버 -> 클라이언트
    EntitySpawn, EntityDespawn                             // TCP, 서버 -> 클라이언트
};
```

| MessageType | 채널 | 방향 | Payload |
|---|---|---|---|
| `RegisterUdpPort` | TCP | C→S | `Port` (uint16) — 이 클라이언트가 UDP를 수신할 포트 |
| `Play` / `Pause` / `Stop` / `Reset` | TCP | C→S | 없음 (헤더만) |
| `SetRate` | TCP | C→S | `DataRateHz` (uint32, 30 또는 60) |
| `EntityControlInput` | UDP | C→S | `EntityId`, `SequenceId`, `Throttle`, `Yaw` (float, -1.0~1.0) |
| `EntityState` | UDP | S→C | `SequenceId`, `Timestamp` (배치 전체에 1개씩) + `EntityCount`(uint16) + `EntityStateEntry`(`EntityId`, `PositionX/Y`, `Heading`, `VelocityX/Y`) × N개 |
| `EntitySpawn` | TCP | S→C | `EntityId`, `Type`, `PositionX/Y`, `Heading` |
| `EntityDespawn` | TCP | S→C | `EntityId` |

`RegisterUdpPortPayloadSize` 같은 상수(`Protocol.h`)가 각 Payload의 정확한 바이트 수를 정의한다 — 수신 측이 헤더 다음에 몇 바이트를 더 읽어야 하는지 알아야 하므로, 이 상수를 절대 struct의 `sizeof`로 대체하면 안 된다(패딩 때문에 실제 전송 크기와 달라질 수 있음).

**`EntityState`는 배치로 묶여 있다** — 수신자 1명당, 그 틱에 Playing인 엔티티 전부를 패킷 하나에 담아 보낸다(`EntityStateEntry`가 24바이트 고정 크기라, 개수만 알면 몇 바이트씩 잘라 읽을지 명확해서 별도 구분자가 필요 없음). 원래는 엔티티마다 패킷을 하나씩 따로 보냈는데, 엔티티 10명·수신자 10명이면 틱당 100개 패킷이 나가는 구조라 수신자당 1개로 묶었다. `SequenceId`/`Timestamp`는 엔티티마다 있는 게 아니라 **패킷(배치) 하나당 1개**다 — 엔티티마다 따로 있으면, 패킷 하나가 유실됐을 때 마치 엔티티 수만큼 유실된 것처럼 통계가 부풀려지기 때문.

## 메시지 흐름 (접속부터 종료까지)

1. **접속**: Client가 TCP로 Server(기본 5000번 포트)에 연결하고, `RegisterUdpPort`로 자신의 UDP 수신 포트를 등록.
2. **Spawn 캐치업**: 등록 성공 시 서버가 (a) 새 클라이언트에게 **자기 자신의 EntitySpawn을 가장 먼저** 보냄 — 클라이언트는 "접속 후 처음 받은 Spawn = 내 EntityId"로 인식(`EntityWorld::TryGetMyEntityId`). (b) 이미 있던 다른 세션들의 Spawn도 전달(늦게 접속해도 기존 참가자가 보이도록). (c) 새 세션의 Spawn을 다른 모든 기존 세션에 브로드캐스트.
3. **Play**: 클라이언트가 `play`를 보내면 그 세션의 `SessionState`가 `Playing`이 됨. Playing인 엔티티만 물리 시뮬레이션이 돌고 브로드캐스트를 주고받는다.
4. **조종**: 클라이언트가 `thrust <v>`/`yaw <v>`를 입력하면 자신의 `EntityId`(첫 Spawn으로 알게 된 값) + 보낼 때마다 증가하는 `SequenceId`를 함께 실어 UDP로 `EntityControlInput`을 서버에 보냄. 서버는 `SessionManager::GetSession(EntityId)`로 바로 세션을 조회하고(O(1), 전체 세션을 순회하지 않음), 발신 IP:포트가 그 세션이 등록해둔 UDP 엔드포인트와 일치하는지만 확인(다른 세션 사칭 방지). `ClientSession::ApplyControlInput`은 `SequenceId`가 마지막으로 적용한 값보다 새로울 때만 반영해서, UDP 역전으로 오래된 입력이 늦게 도착해 최신 입력을 덮어쓰는 걸 막는다.
5. **물리 + 브로드캐스트**: 서버의 `UdpStreamingService`가 60Hz 틱마다: (a) Playing 상태인 모든 엔티티의 `StepPhysics(dt)` 호출 → (b) Playing 상태인 엔티티들의 상태를 한 번만 모아둠 → (c) Playing 상태인 각 수신자에게, 모아둔 엔티티 상태 전부(자기 자신 포함)를 담은 `EntityState` 배치 패킷 1개를 전송. 수신자가 30Hz를 선택했으면 홀수 틱은 건너뜀.
6. **연결 종료**: 클라이언트가 `quit`하거나 연결이 끊기면, 서버가 그 세션의 `EntityDespawn`을 남은 모든 세션에 브로드캐스트하고 세션을 제거.

## 물리 모델 (`ClientSession::StepPhysics`)

```cpp
heading += yaw * YawRateMax(2 rad/s) * dt
speed += throttle * Accel(20/s²) * dt
speed *= clamp(1 - DragPerSecond(1.0) * dt, 0, 1)   // 감속 - throttle 0이면 서서히 정지
speed = clamp(speed, -MaxSpeed(50), MaxSpeed(50))
velocityX = cos(heading) * speed;  velocityY = sin(heading) * speed
positionX += velocityX * dt;       positionY += velocityY * dt
```

`DragPerSecond`(현재 1.0)가 클수록 빨리 감속한다 — `thrust 0`을 주면 관성으로 계속 미끄러지지 않고 대략 1초 시간상수로 속도가 줄어든다.

**CPU 지연으로 틱이 밀려도 시뮬레이션 시간이 뒤처지지 않는다** — `TimerCompensator::WaitForNextTick()`은 예정된 틱을 지나쳤으면 그만큼(`elapsedTicks`)을 반환하고, `UdpStreamingService::Worker`는 그 값만큼 `StepPhysics`를 반복 호출한다. 이렇게 안 하면 건너뛴 틱의 시간이 그냥 사라져서, CPU 지연이 반복될수록 시뮬레이션 시계가 실시간보다 계속 뒤처지게 된다.

## 클라이언트 REPL 명령어

```
play / pause / stop / reset   상태 전환 (서버로 TCP 전송)
30 / 60                       수신 주기 변경 (Hz)
thrust <-1..1>                조종 입력: 전/후진 추력
yaw <-1..1>                   조종 입력: 좌/우 회전
entities                      현재 알려진 엔티티(헬기) 목록과 위치/방향/속도 출력
stats                         유실률/간격/지연 통계를 실시간 갱신 화면으로 표시 (아무 키나 눌러 정지)
quit                          종료
```

## 설정 파일

`Server/server.ini`, `Client/client.ini` (key=value, `#`/`;` 주석). 파일이 없으면 코드에 박힌 기본값(`Common/Config.h`)으로 동작한다.

```ini
# server.ini
TcpPort=5000
UdpPort=6500      # 조종 입력을 받는 포트

# client.ini
Host=127.0.0.1
TcpPort=5000
UdpPort=6000      # 이 클라이언트가 EntityState를 받는 포트 (실행 인자로도 덮어쓸 수 있음: Client.exe 6001)
ServerUdpPort=6500
```

Visual Studio에서 F5로 실행하면 작업 디렉터리가 프로젝트 폴더라 위 파일이 바로 인식된다. 빌드된 exe를 다른 폴더에서 직접 실행하려면 그 폴더에도 ini 파일을 복사해야 한다.

## 동시성 설계 메모

- **TCP**: 접속마다 `ControlWorker` 스레드를 `detach()`(벡터에 안 쌓아둠). 살아있는 스레드 수는 `activeControlWorkerCount_` 원자 카운터로만 추적하고, `Stop()`은 그 값이 0이 될 때까지 짧게 폴링해서 기다린다.
- **세션 조회/변경**: `SessionManager`는 `shared_timed_mutex`로 조회는 병렬(`shared_lock`), 추가/삭제만 배타적(`unique_lock`).
- **엔티티 상태**: `ClientSession`의 Position/Heading/Velocity/Throttle/Yaw는 전용 `entityMutex_`로 보호 (물리 틱 스레드, 조종 입력 수신 스레드, TCP 스레드가 동시에 접근).
- **블로킹 해제 패턴**: 소켓을 다른 스레드에서 `Close()`하면 블로킹 중인 `recv`/`ReceiveAll`이 즉시 에러로 풀려나온다 — `Stop()` 계열 함수들이 전부 이 패턴을 씀.

## 알려진 한계 / 설계상 트레이드오프

- **지연(latency) 측정은 같은 컴퓨터에서 실행할 때만 유효** — `Timestamp`는 `steady_clock` 기반이라 서로 다른 물리 PC끼리는 시계 기준이 달라 값이 의미 없어짐 (진짜 크로스 머신 지연을 재려면 NTP 비슷한 시계 동기화가 별도로 필요, 아직 미구현).
- **`missingSequenceIds_`(유실 판정 집합)** 는 `MissingSequenceWindow`(1000개, 60Hz 기준 약 16초)보다 오래된 항목을 `confirmedLostCount_`로 흡수해서 무한정 커지지 않게 함.
- **EntityState 배치에 개수 상한이 없음** — 수신자당 패킷 1개로 묶긴 하지만, 엔티티 수가 아주 많아지면(대략 60개 이상) 패킷이 UDP 단편화(fragmentation) 없이 안전한 크기(~1400바이트)를 넘어설 수 있음. 소규모 인원 기준으론 문제없고, 나중에 필요하면 "한 패킷에 최대 N개까지만 담고 넘치면 나눠 보낸다" 정도만 추가하면 됨.
- **UDP 조종 입력은 클라이언트가 등록한 그 소켓에서만 보냄** — 서버가 `EntityId`로 세션을 조회한 뒤 발신 IP:포트가 그 세션의 등록된 UDP 엔드포인트와 일치하는지 확인하므로, 클라이언트의 UDP 소켓이 바뀌면(재시작 등) 다시 `RegisterUdpPort`부터 해야 함.

## 다음 단계 (미착수)

`ClientCore` 정적 라이브러리 분리는 완료. 다음은:
1. **C++/CLI Wrapper** (`ClientCore.Managed.dll`, `/clr`) — `ref class ManagedGameClient`가 네이티브 `GameClient*`를 pimpl로 감싸고, `EntityInfo`/`MetricsSnapshot`을 대응하는 managed POCO로 변환. 이벤트 없이 폴링 방식(WPF가 타이머로 `GetEntities()`/`GetMetrics()` 호출). 상호운용 단순화를 위해 .NET Framework(예 4.8) 타깃 권장.
2. **C# WPF 앱** — MVVM. Throttle/Yaw는 슬라이더(키보드 실시간 홀드 아님, 드래그해서 값 설정), 2D 화면은 카메라가 헬기를 따라가지 않는 고정 전체 맵 뷰(월드 좌표를 캔버스에 고정 배율로 매핑, 경계 벗어나면 clamp). `DispatcherTimer`로 주기적으로 갱신.
