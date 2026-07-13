# MMOServer 에이전트 운영 지침

이 파일은 이 저장소를 이어서 작업하는 에이전트를 위한 기준 문서다. 현재 코드는
Linux 이식과 네트워크 안정성 보강이 진행 중인 dirty worktree일 수 있다. 문서보다
실제 소스와 `git status`가 우선이며, 확인하지 않은 부하 테스트 결과나 크래시 원인을
사실처럼 기록하지 않는다.

## 작업을 시작할 때

1. `git status --short`와 `rg --files`로 현재 상태를 확인한다.
2. 관련 소스, 루트 `CMakeLists.txt`, 하위 `CMakeLists.txt`를 함께 읽는다.
3. 기존 수정과 새 파일은 사용자 작업으로 간주한다. 관계없는 변경을 되돌리거나
   포맷팅하지 말고, `git reset --hard`, `git checkout --`, 임의 삭제를 하지 않는다.
4. 이 문서에 적힌 수치와 구조가 코드와 달라졌다면 코드를 기준으로 문서를 갱신한다.

특히 다음 두 파일을 현재 경로로 오인하지 않는다.

- `ServerCore/Network/NetworkSession_bak.cpp`는 과거 참고본이며 CMake 서버 타깃에서
  제외되어 있다. 수정·빌드·분석의 기준으로 삼지 않는다.
- `ServerCore/Memory/Protocol.h`의 5-byte `NetHeader`와 2-byte `LanHeader`는 현재
  활성 TCP 프레임 헤더가 아니다. 현행 wire format은 아래의 FlatBuffers 4-byte
  size prefix다.

## 저장소 구조와 빌드 기준

현재 주요 구조는 다음과 같다.

```text
MMOServer/
├─ CMakeLists.txt                 Linux/크로스 플랫폼 빌드의 최상위 기준
├─ BUILD_LINUX.md                 Linux 빌드와 sanitizer 명령
├─ ServerCore/
│  ├─ CMakeLists.txt              ServerCore 정적 라이브러리 source list
│  ├─ Network/                    Asio 서버·세션·프레임·JobQueue/Dispatcher
│  ├─ Memory/                     RingBuffer와 Windows allocator/lock-free 실험
│  ├─ Dump/                       Windows minidump, Linux signal backtrace 보조
│  ├─ Test/                       CTest에 등록된 네트워크 테스트 4종
│  └─ Utill/                      로그·시간·UID 등(디렉터리 철자 유지)
├─ PlayServer/                    실제 서버 실행 파일과 메트릭 기록
├─ SoakClient/                    Linux 장시간 봇과 실제 서버용 ProtocolProbe
├─ DummyClient/                   기존 봇 클라이언트/Visual Studio 프로젝트
├─ ThirdParty/FlatBuffers/        MMOServer가 직접 관리하는 커스텀 flatc/runtime 소스
├─ flatbuffers/                   스키마, 생성 헤더/runtime, 검증된 Windows flatc.exe
├─ include/                       bundled spdlog/pqxx 및 개발자 로컬 의존성
├─ db/DBSchema/                   DB 스키마와 생성 도구
├─ docs/                          구조 설명 자료
├─ scripts/                       Linux/Docker soak 실행·수집·판정·core 분석
├─ Dockerfile                     Ubuntu 24.04 빌드/테스트/실행 이미지
├─ compose.soak.yml               server/loadbot/preflight 컨테이너 구성
└─ MemoryPoolBenchmark/           allocator 벤치마크 결과 이미지
```

- Linux 서버에서 source-of-truth는 루트와 하위의 **CMake source list**다.
  `.sln`, `.vcxproj`, 루트의 기존 `.lib` 파일은 Windows 개발/과거 산출물이며 Linux
  서버에 포함됐다는 근거가 아니다.
- 현재 루트 CMake 그래프는 `ServerCore` 정적 라이브러리, `PlayServer`,
  `SoakClient`, `ProtocolProbe`, CTest 실행 파일을 만든다. `DummyClient`는 기존
  Visual Studio 경로이며 Linux soak의 기준으로 사용하지 않는다.
- Boost.Asio는 헤더 기반으로 사용한다. `include/boost/asio.hpp`가 있으면 로컬
  헤더를, 없으면 CMake가 시스템 Boost를 찾는다. spdlog 헤더는 저장소 것을 쓴다.
- 커스텀 FlatBuffers의 source-of-truth는 `ThirdParty/FlatBuffers`다. 이 디렉터리는
  submodule/subtree가 아니라 MMOServer Git이 직접 추적하는 일반 소스다. 출처와
  커스텀 내역은 `ThirdParty/FlatBuffers/MMOSERVER_CUSTOM.md`를 따른다.
- `flatbuffers/flatc.exe`, `flatbuffers/flatbuffers/` runtime header와
  `flatbuffers/ProtocoID.h`는 source-of-truth에서 만든 추적 대상 산출물이다. 임의로
  다운로드한 `flatc.exe`만 교체하거나 compiler/runtime/generated code 버전을
  따로 올리지 않는다. Windows에서는 `scripts/build_custom_flatbuffers.ps1` 또는
  CMake의 `MMOCustomFlatc` target으로 셋을 함께 갱신한다.
- MMOServer의 `MessageID`는 `union MessageID : int`를 사용한다. 커스텀 C# generator가
  explicit union underlying type을 보존해야 하며, compiler 갱신 때 C++와 C# 생성을
  모두 검증한다. C++ generated header의 FlatBuffers version assertion은 bundled
  runtime version과 일치해야 한다.
- `Memory/MemoryPool.cpp`와 관련 PCH는 현재 `MSVC`에서만 `ServerCore`에 들어간다.
  커스텀 allocator의 128-bit CAS와 주소 배치 가정에 대한 이식성 검증 전에는
  Linux 서버 경로에 억지로 넣지 않는다. 활성 네트워크 객체는 `std::make_shared`
  기반이다.

## 현재 서버 런타임 구조

```text
TCP accept/connect/read/write
        ↓
NetworkServer의 boost::asio::io_context + I/O worker jthread들
        ↓
NetworkSession별 strand (소켓, recv 처리, 실제 send deque, close 직렬화)
        ↓
FrameDecoder → XOR 복호화 → FlatBuffers verifier/unpack
        ↓
세션별 JobQueue → JobDispatcher logic thread → PlayServer handler
        ↓
NetworkSession::Send → admission lock → strand → async_write
```

### 스레드와 직렬화 계약

- `NetworkServer`는 하나의 `io_context`와 worker `std::jthread`들을 가진다. worker
  수를 0으로 지정하면 하드웨어 동시성 기준 1~4개로 결정된다.
- 한 `NetworkSession`의 resolver/socket, receive decode, 실제 send queue 및 close
  상태는 그 세션의 Asio `strand`에서 직렬화한다.
- 여러 logic thread에서 동시에 `Send()`할 수 있으므로, strand에 post하기 전의
  송신 허용 여부와 pending-byte 예약은 `m_sendAdmissionLock`이 보호한다. 이 예약을
  단순 atomic check/add로 약화하지 않는다.
- 서버의 session map은 별도 mutex, 원격 endpoint 문자열은 endpoint mutex로
  보호된다. strand가 모든 전역 자료구조까지 보호한다고 가정하지 않는다.
- 수신된 패킷은 세션별 `JobQueue`로 들어간다. 한 세션의 queue는 한 번에 하나의
  dispatcher worker만 처리하므로 그 세션의 handler 순서와 비동시성이 보장된다.
  서로 다른 세션은 병렬 처리될 수 있고, 같은 세션도 다음 batch는 다른 OS thread가
  처리할 수 있다. 따라서 thread affinity가 아니라 **세션별 직렬 실행**만 계약이다.
- `PlayServer`의 현재 dispatcher 구성은 main 2개, sub 1개 logic thread이며 listen은
  main dispatcher에 연결된다. 이 수치는 코드가 바뀌면 다시 확인한다.

### 수명과 종료 계약

- session은 server map, 호출자, strand/Asio completion이 가진 `shared_ptr` 때문에
  map에서 제거된 뒤에도 잠시 살아 있을 수 있다. `sessions`는 map 항목 수이고
  `session_objects`는 실제 살아 있는 `NetworkSession` 객체 수다. 둘을 같은 지표로
  대체하지 않는다.
- send packet도 `shared_ptr`로 async write 완료까지 살아 있어야 한다. raw buffer나
  stack 객체를 completion보다 먼저 파괴하지 않는다.
- `JobQueue`는 `JobDispatcherHandle`을 공유하고, dispatcher raw pointer는 handle lock
  안에서만 확인한다. dispatcher 파괴 시 handle을 먼저 null로 만들어 dangling
  접근을 막는다. 이 계약을 우회해 raw `JobDispatcher*`를 장기 보관하지 않는다.
- handler가 참조하는 저장소는 dispatcher thread보다 오래 살아야 한다.
  `JobDispatcher`는 생성자에서 즉시 thread를 시작하므로 **멤버 선언 순서가 생성과
  파괴 양쪽의 수명 계약**이다. 현재 `PlayServer`의 handler map과 handler가 접근하는
  `m_recvAuthenCount`는 dispatcher보다 먼저 선언되어 dispatcher join 뒤에 파괴된다.
  새 handler state도 반드시 이 순서를 지킨다. shutdown 뒤에도 logic job이 남을
  가능성을 없애거나 dispatcher를 명시적으로 stop/drain해야 하며, “network shutdown이
  끝났으니 logic queue도 비었다”고 검증 없이 가정하지 않는다.
- 정상 종료 순서는 **control thread에서 부하 발생 중지 →
  `NetworkServer::Shutdown()` 반환 → dispatcher 파괴 → handler/state 파괴**다.
  반드시 dispatcher destruction보다 `Shutdown()`이 먼저다.
- `Shutdown()`을 Asio worker에서 호출하면 안 된다. 현재 구현도 이를 거부한다.
  shutdown은 acceptor와 session close를 post하고 work guard를 해제한 뒤 worker를
  join하며, `io_context.stop()`으로 completion을 잘라내지 않고 close/aborted
  completion을 drain하는 구조다. 이 순서를 바꾸면 queued handler가 닫힌 session을
  되살리거나 객체가 남을 수 있으므로 race test 없이 변경하지 않는다.
- close는 여러 오류 경로에서 중복 요청될 수 있다. `m_closeStarted` 기반 idempotency와
  “close 이후 send post 금지” 계약을 유지한다.
- `RegisterPacket` 계열은 FlatBuffers native union의 ownership을 handler 쪽
  `shared_ptr`로 넘기면서 holder의 type을 `NONE`으로 바꾸는 기존 수명 규칙이 있다.
  소유권 동작을 이해하지 않고 이 두 줄 중 하나만 제거하지 않는다.

## 현행 패킷 규격

Wire frame은 다음과 같다.

```text
[4-byte little-endian flatbuffers::uoffset_t body_size]
[body_size bytes: XOR 변환된 FlatBuffers MessageHolder 본문]
```

- `PACKET_HEADER_SIZE == 4`이고 FlatBuffers `FinishSizePrefixed()`가 만든 prefix다.
- XOR은 헤더를 제외한 body에만 적용하며 `NetworkServer::Convert()`를 송수신에 같은
  방식으로 사용한다. 고정 키 XOR은 난독화일 뿐 운영 보안/암호화로 간주하지 않는다.
- `RingQ::RING_BUFFER_SIZE == 7000`이고 한 칸을 비우므로 최대 **전체 frame은
  6999 bytes**, 최대 **body_size는 6995 bytes**다.
- header가 6995 이하인데 body가 아직 덜 왔으면 decoder는 `NeedMoreData`로
  기다린다. TCP fragmentation은 정상이며, 한 read에 여러 frame이 합쳐지는 것도
  정상이다.
- body_size 6996 이상, 손상된 FlatBuffer, `NONE`/알 수 없는 message type은 protocol
  error로 해당 session을 닫아야 한다. 정상 session으로 오류가 전파되면 안 된다.
- 수신 decoder와 송신 admission 모두 6999-byte frame 상한을 강제한다. outbound가
  이를 넘으면 wire에 쓰지 않고 `Send()`가 `false`를 반환한다. 호출자는 이 반환을
  무시한 채 성공 packet으로 세지 않는다.
- 현재 Linux socket은 `TCP_NODELAY`, `SO_KEEPALIVE`, 대략 idle 30초 + 10초 간격 3회
  probe를 설정한다. frame의 첫 일부가 들어온 뒤 10초 안에 조립이 끝나지 않으면 해당
  session을 protocol error로 닫는다. TCP keepalive는 black-hole peer를, frame deadline은
  partial-frame slowloris를 겨냥하며 로그인/게임 heartbeat를 대신하지는 않는다.

## Flow control과 관측 지표

`NetworkServer::Initialize()`의 현재 기본 상한은 다음과 같다.

| 대상 | 기본값 | 초과 시 동작 |
|---|---:|---|
| session별 pending send bytes | 256 KiB | admission 중단, backpressure count 증가, session 종료 |
| session별 pending receive jobs | 256 | push 거부, 해당 session 종료 |
| 전체 session map | 4096 | 새 session 생성 거부 |
| 한 receive frame | 6999 bytes | 더 큰 length header를 protocol error로 거부 |

`PlayServer`가 metrics CSV를 열면 현재 다음을 기록한다: `sessions`,
`session_objects`, created/removed sessions, recv/send TPS와 누계/bytes, handler의
auth 누계, protocol/network error, backpressure disconnect, RSS, virtual bytes.

- recv count는 유효한 패킷이 receive JobQueue에 들어간 뒤 증가한다.
- send count는 `async_write`가 성공 완료된 뒤 증가한다.
- Linux의 `virtual_bytes`는 `/proc/self/status`의 `VmSize`다. Windows commit size와
  동일한 의미가 아니며, `VmSize` 증가 하나만으로 메모리 누수를 확정하지 않는다.
  RSS, anonymous/private memory, 객체/FD 수, sanitizer 결과를 함께 본다.
- 정상적으로 외부 peer가 FIN/RST를 보내도 현재 server의 `network_errors`가 증가할
  수 있다. churn에서 예상한 disconnect와 예상하지 않은 오류를 run log로 분류한다.
- `scripts/analyze_soak.py`는 server CSV와 하나 이상의 client shard CSV를 합쳐 JSON
  report를 만든다. 기본 synthetic gate는 offered 5000 TPS의 95%인 방향별 4750 TPS와
  same-host RTT p95 50 ms다. threshold와 명령 전체를 artifact에 남기며 일반 `PASS`와
  실제 24시간·peak connected 1000의 `QUALIFIED`를 혼동하지 않는다.

## CTest 4종과 한계

Linux 변경은 최소한 아래 네 테스트를 모두 실행한다.

| 테스트 | 현재 검증하는 것 | 검증하지 못하는 것 |
|---|---|---|
| `FrameDecoderTests` | partial header/body, coalescing, ring wrap, size matrix, 6995 body와 +1 거부 | 실제 TCP/XOR/FlatBuffers handler, 장시간 수명 |
| `NetworkIntegrationTests` | loopback accept/connect, 8×64 concurrent send와 seq 중복, disconnect, active shutdown race, 재초기화, session cap, send backpressure | 별도 프로세스·컨테이너, 1000명/24시간, 실제 RTT·RSS·FD 추세 |
| `FlowControlTests` | pending JobQueue 상한, dispatcher 파괴 뒤 push/get 거부 | 실제 I/O, 다중 worker 장시간 경합, send flow control |
| `DummyNetworkTests` | exact request/ACK, 여러 payload 크기와 reconnect, 실제 TCP fragmentation/coalescing, truncated/over/malformed/암호화 오류 격리 | 대규모/장시간 TPS·latency·memory, 별도 프로세스 장애 |

이 네 테스트가 통과해도 24시간 부하 검증을 대신하지 않는다. 반대로 sanitizer
실행 속도를 Release TPS 기준과 비교하지 않는다.

## Linux/Docker 장시간 검증 규칙

### 빌드와 재현 정보

- 최종 배포와 같은 Linux 또는 Linux container에서 검증한다. Windows loopback
  결과만으로 Linux 준비 완료로 판정하지 않는다.
- TPS용 `RelWithDebInfo`/Release, ASan, TSan은 서로 다른 build directory와 실행으로
  분리한다. CMake도 ASan+TSan 동시 사용을 금지한다.
- 24시간 TPS run은 sanitizer 없는 binary로 한다. ASan/LSan과 TSan은 overhead를
  고려한 별도 대표 workload로 반복하고 두 sanitizer 결과를 합쳐 쓰지 않는다.
- 매 run마다 `run_id`, git commit과 dirty diff, binary SHA-256/build-id, compiler와
  CMake flags, kernel, container image digest, CPU/memory limit, `ulimit`, server/client
  설정, random seed를 보존한다.
- core 수집이 가능하도록 실행 전에 환경에 맞게 `ulimit -c unlimited`와 host의
  core/coredump 정책을 확인한다. crash 뒤 분석이 끝날 때까지 binary를 재빌드하거나
  core와 다른 binary로 교체하지 않는다.

### 24시간 workload

1. 1000개의 **동시 connected session을 유지**하며 24시간 연속 실행한다. 단순히
   누적 1000 connect를 만든 것을 동접 1000으로 세지 않는다.
   `DURATION_SEC=0`은 SIGINT/SIGTERM까지 계속 실행하는 무제한 모드지만, 합격 자격은
   설정값이 아니라 CSV에 기록된 실제 실행 시간으로 판정한다. 최종 표본과 ACK drain을
   보존하도록 강제 종료 대신 정상 종료한다.
2. 초기 provisional packet budget은 client당 5 request/s다. 따라서 steady state의
   server 기준은 **recv 5000 TPS + 1:1 ACK send 5000 TPS**다. 이것은 영구적인 게임
   SLA가 아니라 첫 안전선이다. 실제 게임의 이동/전투/heartbeat/채팅/broadcast
   budget이 정해지면 방향별 합계로 다시 계산한다.
3. 정상 packet은 작은 것부터 near-limit까지 섞는다. 현재 `SoakClient`의 token
   payload matrix는 1, 32, 128, 512, 1024, 4096, 6400 bytes이며,
   **FlatBuffers 직렬화 뒤 실제 wire frame 크기**를 기록하고 6999 이하인지 검사한다.
   payload 길이와 frame/body 길이를 혼동하지 않는다.
4. raw TCP bot으로 header를 1/1/2 bytes와 random chunk로 쪼개는 fragmentation,
   여러 독립 frame을 한 write로 묶는 coalescing을 계속 섞는다. TCP read 경계가 packet
   경계라고 가정하는 bot을 만들지 않는다.
5. graceful FIN, RST, 연결 직후 종료, 송신 중 종료, 무작위 지연 후 재접속을 반복한다.
   churn 중에도 controller가 목표 동접을 회복해야 한다. bot/session generation과
   reconnect seed를 기록해 stale ACK를 새 연결의 ACK로 세지 않는다.
6. 낮은 비율의 격리된 raw connection으로 다음 negative matrix를 실행한다.
   - under: 정상 length header 뒤 body 일부만 전송하고 FIN/RST
   - persistent under/slowloris: header/body를 아주 천천히 전송
   - over: body length 6996, `UINT32_MAX`
   - malformed: zero body, random body, 손상된 root offset, XOR하지 않은 유효 frame,
     `NONE`/unknown message type, 잘못된 checksum metadata
   `ProtocolProbe`는 이 negative matrix를 **별도 preflight 서버**에 실행한다. 예상
   protocol-error count가 healthy soak의 zero-error gate를 오염시키지 않도록 preflight
   서버를 종료한 뒤 새 서버 프로세스로 24시간 run을 시작한다.
7. 각 정상 request에는 연결 generation 안에서 유일한 `seq`와 payload checksum을
   부여한다. 현재 `PlayServer`는 `connectSessionKey`가
   `loadtest-fnv1a64:<16-hex>`이면 `accounttoken`의 FNV-1a 64-bit 값을 검사하고 ACK에
   같은 `seq`와 success/invalid 결과를 보낸다. bot은 `(bot, generation, seq)`별 원본
   checksum을 보관해 exact ACK, RTT, 누락, 중복, stale ACK를 판정한다. 현재 ACK
   schema는 checksum 자체를 echo하지 않으므로 checksum echo까지 요구하면 schema와
   양쪽 코드를 먼저 확장해야 한다.

### 유령 session과 FD 판정

run 중과 churn cooldown 뒤 다음 값을 같은 timestamp로 비교한다.

- bot의 connected/connecting/disconnected 수와 server `sessions`
- `created_sessions - removed_sessions == sessions`
- server `sessions`와 `session_objects`; in-flight completion이 끝난 cooldown 뒤에는
  불명확한 초과 객체가 없어야 한다.
- `/proc/<pid>/fd` 및 `ss`의 socket 수. listener/log용 시작 baseline을 따로 저장하고,
  churn 뒤 socket FD가 그 baseline + 정상 동접 수로 돌아오는지 본다.
- 정상 shutdown/reinitialize test에서는 cooldown 뒤 `sessions == 0`,
  `session_objects == 0`, pending send bytes 0, socket FD가 시작 baseline으로 복귀해야
  한다.

순간 snapshot 하나로 유령 session을 판정하지 않는다. 최소 두 keepalive 주기 또는
명시한 cooldown 동안 같은 orphan이 남고, bot/server/FD 세 관측이 불일치하는지 본다.

### 초기 합격 기준

아래 값은 실제 게임 packet budget과 latency SLO가 생기기 전의 **provisional local
Linux/Docker gate**다. 하드웨어와 container limit을 결과에 반드시 붙인다.

- 처리량: warm-up 뒤 active-bot 시간으로 계산한 방향별 target의 95% 이상을 모든
  1분 window에서 유지한다. 1000×5 pps run이면 server recv/send 각각 4750 TPS 이상,
  전체 exact ACK는 최종 drain 뒤 100%여야 한다.
- RTT: 정상 packet의 p95 ≤ 50 ms, p99 ≤ 100 ms, p99.9 ≤ 250 ms. churn/negative
  traffic의 RTT는 정상 steady-state와 분리 집계한다.
- 정확성: 정상 request/ACK의 loss, duplicate, checksum mismatch, unexpected/stale ACK
  각각 0. crash, assert, hang, healthy sentinel 단절도 0이다. 정상 workload의
  backpressure disconnect는 0이어야 한다. 의도한 malformed/churn 오류는 사전 예상
  수와 대조하며 generic error counter가 0이라고만 요구하지 않는다.
- 메모리: 첫 1시간을 warm-up으로 제외한다. 5분 median RSS와 가능하면 anonymous/private
  memory를 사용해, 마지막 6시간 median이 앞선 안정 6시간 median보다
  `max(64 MiB, 5%)` 넘게 높지 않고 마지막 12시간 추세가 1 MiB/hour 이하인지 본다.
  기준 초과나 계단식 무한 상승은 실패/조사 대상으로 둔다. `VmSize`만으로 누수를
  확정하지 않으며 별도 ASan/LSan run의 leak/use-after-free는 0이어야 한다.
- 자원 수명: cooldown 뒤 설명되지 않는 session object와 socket FD 증가가 0이어야
  한다. TSan run에서 data-race 보고도 0이어야 한다.

현재 analyzer를 쓸 때는 적어도 `--min-recv-tps 4750 --min-send-tps 4750`과 이 run에
정한 latency/memory threshold를 명시한다. script의 `status: PASS` 또는 exit code 0만
보지 말고 JSON의 `coverage.qualification_24h_1000_clients.qualified == true`도
확인한다. 짧은 smoke run은 통계 check가 PASS일 수 있어도 24h/1000명 검증 통과가
아니다. analyzer가 아직 직접 강제하지 않는 1분-window TPS, p99/p99.9, 상대 RSS와
시간당 slope, FD cooldown 기준은 별도 계산 결과를 함께 보존한다.

합격 기준을 낮춰 테스트를 통과시키지 않는다. 환경 노이즈로 조정할 때는 기존 값,
관측 근거, 새 값을 run 문서에 함께 남긴다.

## 크래시가 발생하면

1. workload를 무작정 재시작하기 전에 core, **그 core를 만든 동일 binary**, server와
   client log/metrics, config/seed, container와 host 정보를 보존한다.
   Linux `CrashDump` helper는 signal backtrace를 stderr에 남기고 signal을 다시
   발생시키는 보조 장치일 뿐, 자체적으로 Windows식 full dump를 저장하지 않는다.
   실제 core 유무는 host/container의 core 정책으로 확인한다.
2. 동일 binary와 symbol로 `gdb`에서 crashing thread뿐 아니라 모든 thread의 stack을
   확인한다. OOM kill이면 core가 없을 수 있으므로 kernel/cgroup 기록을 확인한다.
3. 확인된 사실, 가설, 검증 결과를 분리한다. stack 한 줄만 보고 race나 메모리 누수로
   단정하지 않는다.
4. `troubleshoot.md`에 사고 템플릿을 append한다. 재현·수정·ASan/TSan·24시간 재검증이
   끝나기 전에는 “해결”로 표시하지 않는다.
