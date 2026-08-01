# NetworkControl

TCP로 제어하고 UDP로 고주기 데이터를 주고받는 서버/클라이언트 네트워크 시스템. **2D 멀티플레이어 헬기 시뮬레이션**(서버 권위 물리)을 구현했고, Client 네트워킹 로직을 재사용 가능한 라이브러리(`ClientCore`)로 뽑아낸 뒤 C++/CLI Wrapper(`ClientCore.Managed`)를 거쳐 C# WPF GUI(`Gui`)까지 얹은 상태.

## 기술 스택 / 빌드 환경

- Windows, Visual Studio 2022 (v143 toolset), C++17 이전 표준(`std::clamp` 등 C++17 전용 stdlib 함수는 못 씀 — 프로젝트 표준을 올리지 않고 직접 만든 헬퍼로 대체하는 관례)
- 순수 Winsock2 기반, 외부 라이브러리 의존성 없음 (JSON/vcpkg 패키지 미사용)
- 솔루션 2개: `Server/Server.sln`, `Client/Client.sln`(+ `ClientCore.vcxproj`, `ClientCore.Managed.vcxproj`를 프로젝트 참조로 포함). `Common/` 소스는 각 최상위 프로젝트(`Server`, `ClientCore`)가 직접 컴파일 목록에 포함해서 링크한다 — **Common에 파일을 추가하면 `Server.vcxproj`와 `ClientCore.vcxproj` 양쪽에 수동으로 `ClCompile`/`ClInclude` 항목을 추가해야 한다** (자동으로 안 잡힘, 이 프로젝트에서 반복적으로 겪은 이슈).
- `ClientCore`는 `ClientCore.Managed`(C++/CLI, `/clr`)에서도 링크되므로 **동적 CRT(`MultiThreadedDLL`/`MultiThreadedDebugDLL`)로 명시적으로 고정**되어 있다 (`/clr`은 정적 CRT와 호환 안 됨) — `Client.vcxproj`도 링크 대상과 맞추기 위해 동일하게 맞춰둠.

## 프로젝트 구조

```
Common/               서버·클라이언트가 공유하는 프로토콜, 소켓 RAII 래퍼, 타이머, 설정 로더
Server/               TCP 제어 + UDP 물리 시뮬레이션/브로드캐스트 콘솔 앱
ClientCore/           클라이언트 네트워킹 로직 (정적 라이브러리) - 콘솔/GUI가 공유
ClientCore.Managed/   C++/CLI wrapper (.dll) - GameClient를 C#에서 쓸 수 있게 감쌈
Client/               ClientCore를 쓰는 콘솔 REPL (GameClient 하나 + 명령 파싱/출력)
Gui/                  ClientCore.Managed를 쓰는 C# WPF GUI (MVVM)
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
| ClientCore.Managed | `ManagedGameClient.h` / `.cpp` | `GameClient`를 감싼 `ref class` — `String^`↔`std::string` 등 타입 변환, `IDisposable`(`~T`/`!T`) 패턴 |
| ClientCore.Managed | `ManagedTypes.h` | `ManagedEntityInfo`/`ManagedMetricsSnapshot` — 네이티브 구조체를 그대로 옮긴 C# 바인딩용 POCO |
| Client | `Main.cpp` | 진입점 + 대화형 REPL (`GameClient` 하나 생성 + 명령 파싱/화면 출력만 담당) |
| Gui | `ViewModels/MainViewModel.cs` | `ManagedGameClient` 소유, 연결/재생 제어/조종 커맨드, `DispatcherTimer`로 `GetEntities()`/`GetMetrics()` 폴링(30Hz) |
| Gui | `ViewModels/EntityViewModel.cs` | 엔티티 1개의 표시 상태 - 월드 좌표를 고정 배율 캔버스 좌표로 매핑(`WorldExtent` 밖은 clamp) |
| Gui | `Views/MainWindow.xaml` | 연결 입력, Play/Pause/Stop/Reset·Hz 버튼, Throttle/Yaw 슬라이더, 고정 전체 맵 Canvas, 수신 통계 |
| Gui | `RelayCommand.cs` | WPF 기본 제공이 없는 `ICommand` 구현체 |

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

## C++/CLI Wrapper (`ClientCore.Managed`)

### 왜 필요한가

WPF는 C#(.NET) 세계에서 돌고, `ClientCore.lib`는 순수 네이티브 C++이라 WPF가 직접 호출할 수 없다. **C++/CLI**는 네이티브 C++과 .NET을 한 파일 안에 같이 쓸 수 있는 특수한 C++ 방언이라, 이 둘을 잇는 "다리" 역할의 DLL을 만드는 데 쓴다. 하는 일은 로직을 새로 짜는 게 아니라, `Client::GameClient`(네이티브)를 C#이 이해할 수 있는 타입으로 그대로 감싸서 다시 내보내는 것뿐이다.

### 프로젝트 설정

`ClientCore.Managed.vcxproj`는 다른 프로젝트들과 성격이 다르다.
- `<ConfigurationType>DynamicLibrary</ConfigurationType>` — `.dll` 하나를 만듦
- `<CLRSupport>true</CLRSupport>` — `/clr` 컴파일러 스위치 (이 프로젝트 소스에서 네이티브 코드와 매니지드 코드를 같이 쓸 수 있게 함)
- `<TargetFrameworkVersion>v4.8</TargetFrameworkVersion>` — .NET Framework 4.8 타깃 (최신 .NET Core/5+ 보다 C++/CLI와 상호운용이 훨씬 무난함)
- `ClientCore.vcxproj`를 프로젝트 참조로 추가 (네이티브 `GameClient`에 접근하기 위해)

### 코드 구조

**`ManagedTypes.h`** — 네이티브 구조체를 그대로 옮긴 값 전달용 POCO(Plain Old CLR Object).
```cpp
public ref class ManagedEntityInfo
{
public:
    property System::UInt32 EntityId;
    property System::Single PositionX;
    property System::Single PositionY;
    property System::Single Heading;
    property System::Single VelocityX;
    property System::Single VelocityY;
};
```
`property Type Name;`은 C++/CLI의 auto-property 문법(C#의 자동 구현 속성과 동일). `ManagedMetricsSnapshot`도 같은 방식으로 `Client::MetricsSnapshot`을 그대로 옮김.

**`ManagedGameClient.h`/`.cpp`** — 네이티브 `Client::GameClient*`를 pimpl로 들고 있는 `ref class`.
```cpp
public ref class ManagedGameClient
{
public:
    ManagedGameClient();
    ~ManagedGameClient();   // IDisposable::Dispose - 결정적 해제
    !ManagedGameClient();   // 파이널라이저 - Dispose를 안 불렀을 때의 안전망

    bool Connect(System::String^ host, int tcpPort, int udpPort, int serverUdpPort);
    bool Play(); bool Pause(); bool Stop(); bool Reset();
    bool SetRate30(); bool SetRate60();
    bool SendControlInput(float throttle, float yaw);

    System::Nullable<System::UInt32> GetMyEntityId();
    System::Collections::Generic::List<ManagedEntityInfo^>^ GetEntities();
    ManagedMetricsSnapshot^ GetMetrics();

private:
    Client::GameClient* native_;
};
```
- **`~T()`/`!T()` 패턴**: `~ManagedGameClient()`(소멸자)가 `.NET`의 `Dispose()`로 컴파일되고, `!ManagedGameClient()`(파이널라이저)가 GC가 못 챙긴 경우의 안전망이 된다. 관례대로 `~T() { this->!T(); }`로 소멸자가 파이널라이저를 호출해서, 어느 경로로 정리되든 네이티브 객체(`delete native_`)가 한 번만 해제되게 한다. 컴파일러가 `IDisposable` 전체 패턴(`SuppressFinalize` 포함)을 자동으로 만들어준다.
- **폴링 방식, 이벤트 없음**: 서버가 뭘 보낼 때마다 알림을 주는 대신, `GetEntities()`/`GetMetrics()`가 그 순간의 스냅샷을 반환한다. WPF가 타이머로 주기적으로 이 함수들을 불러서 화면을 다시 그리는 구조로 쓸 예정.
- **타입 변환**: `System::String^ → std::string`은 `msclr::interop::marshal_as<std::string>(host)`로, `std::vector<EntityInfo> → List<ManagedEntityInfo^>^`는 루프 돌면서 `gcnew`로 하나씩 옮겨 담는 식으로 처리.

### 삽질 기록 (다음에 비슷한 걸 만들 때 참고)

**1) 헤더에 `using namespace System;`을 넣으면 안 됨** — 처음에 헤더 맨 위에 편의상 넣어뒀더니 이런 에러가 났다.
```
error C3699: '*': cannot use this indirection on type 'IServiceProvider'
error C2371: 'IServiceProvider': redefinition; different basic types
```
Windows SDK의 `servprov.h`(COM)가 네이티브 `IServiceProvider`(포인터 `*` 기반)를 선언하는데, `.NET`에도 이름이 같은 `System::IServiceProvider`(핸들 `^` 기반)가 있다. 헤더에서 `using namespace System;`으로 이름을 열어두면, 나중에 이 헤더를 포함한 파일이 네이티브 COM 헤더를 (직접이든 간접적으로든) 끌어들일 때 컴파일러가 두 `IServiceProvider`를 헷갈려서 충돌한다. **고친 방법**: `using namespace System;`을 아예 안 쓰고, `System::String^`, `System::UInt32`, `System::Nullable<...>`처럼 전부 완전한 이름으로 씀. 헤더 파일에는 특히 `using namespace`를 넣지 않는 게 안전.

**2) `/clr`은 정적 CRT(`/MT`)와 호환 안 됨** — `ClientCore.lib`가 매니지드 DLL에서도 링크되므로, CRT 링크 방식(`RuntimeLibrary`)을 `MultiThreadedDLL`/`MultiThreadedDebugDLL`(동적)으로 양쪽 프로젝트(`ClientCore`, `Client`)에 명시적으로 맞춰야 했다. 안 그러면 CRT 심볼이 중복 정의되거나 링크가 아예 안 맞음.

**3) Winsock이 초기화가 안 됨 + `Ws2_32.lib`가 안 잡힘** — 콘솔 앱은 `main()`에서 `Common::SocketRuntime` 객체를 직접 만들어서 `WSAStartup`을 호출하는데, `ManagedGameClient`는 그런 걸 만든 적이 없었다. 게다가 `Ws2_32.lib` 링크는 `SocketRuntime.cpp` 안의 `#pragma comment(lib, "Ws2_32.lib")`로 되는데, 이 파일의 오브젝트 자체가 링크에 안 끌려 들어가니 그 pragma도 적용이 안 돼서 `__imp_socket`, `__imp_bind` 같은 unresolved external이 무더기로 났다. **고친 방법**: `ManagedGameClient.cpp`에 프로세스당 한 번만 초기화되는 `static Common::SocketRuntime` (Meyer's singleton)을 두고 생성자에서 접근하도록 함 — 이러면 `SocketRuntime.obj`가 링크에 끌려 들어가면서 `Ws2_32.lib` 문제도 같이 해결됨.

**4) 관계없는 `draco.lib`와 링크 충돌** — Google의 3D 압축 라이브러리인 `draco.lib`가 `std::bad_alloc`/`std::exception` 심볼을 우리 프로젝트와 중복 정의한다는 링크 에러가 났다. 이 프로젝트는 draco를 전혀 참조한 적이 없어서, vcpkg 전역 통합(`vcpkg integrate install`)이 이 컴퓨터에 예전에 설치해둔 무관한 패키지의 lib 경로를 새 프로젝트에도 끼워 넣은 것으로 추정됨. `<VcpkgEnabled>false</VcpkgEnabled>`를 vcxproj에 추가해서 이 프로젝트만 vcpkg 전역 통합에서 빼서 해결.

## C# WPF GUI (`Gui`)

- **프로젝트 형식**: SDK-style `.csproj` (`net48`, `UseWPF=true`) — 나머지 프로젝트가 쓰는 레거시 vcxproj 스타일과 다르게, WPF 쪽은 SDK-style이 훨씬 짧고 `.xaml`을 자동으로 인식해서 이걸 씀. `ClientCore.Managed.vcxproj`를 `ProjectReference`로 참조(x64만 지원 — `/clr` 매니지드 DLL은 프로세스 비트수가 맞아야 로드되므로 `PlatformTarget`을 `x64`로 고정). `Client.sln`에 세 번째 프로젝트로 추가됨.
- **MVVM**: `MainViewModel`이 `ManagedGameClient` 하나를 들고 있고, Connect/Play/Pause/Stop/Reset/SetRate는 `RelayCommand`로 바인딩. Throttle/Yaw는 슬라이더 값이 바뀔 때마다(드래그 중 계속) `SendControlInput` 호출 — 키보드 실시간 홀드 방식이 아님.
- **폴링**: 이벤트 알림이 없는 `ManagedGameClient` 특성에 맞춰 `DispatcherTimer`(30Hz)가 `GetEntities()`/`GetMetrics()`를 주기적으로 불러 `ObservableCollection<EntityViewModel>`과 `Metrics`를 갱신. 매 틱마다 컬렉션을 비우고 다시 채우지 않고, `EntityId` 기준으로 기존 항목은 갱신·사라진 항목만 제거(Despawn 대응).
- **2D 뷰**: 카메라가 헬기를 따라가지 않는 고정 배율 전체 맵(`EntityViewModel.WorldExtent = ±300` 월드 단위를 640x640 캔버스에 매핑, 벗어나면 가장자리에 clamp). 원, Circle 안쪽 방향선(`RotateTransform`)으로 위치/헤딩 표시, 내 엔티티는 다른 색으로 강조.
- **알려진 한계**: 화면 크기가 고정(`ResizeMode="CanMinimize"`)이고 `WorldExtent`도 상수로 박혀 있어서, 헬기가 그 범위를 벗어나 멀리 날아가면 캔버스 가장자리에 눌러붙어 보임(줌/팬은 아직 없음).
