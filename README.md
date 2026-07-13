# MMOServer

C++20 / Boost.Asio 기반 MMO 서버 인프라와 부하 테스트 도구 모음입니다.
대규모 동시 접속을 가정한 네트워크 / 메모리 / 동기화 계층을 직접 구현해보는 것을 목표로 합니다.

> 이 저장소는 **인프라 학습 및 실험용 코드베이스**입니다.
> 실제 게임 컨텐츠 로직은 아직 최소한만 들어 있고, 네트워크 / 메모리 풀 / Lock-Free 자료구조 / 부하 테스트 같은 **서버 기반 시스템 구현**에 무게가 실려 있습니다.

---

## 한눈에 보는 구조

```
                          ┌──────────────────────────────┐
   클라이언트 ─ TCP ─────▶ │ Boost.Asio I/O Worker       │
                          │  - Windows: IOCP             │
                          │  - Linux: epoll              │
                          │  - 세션별 strand             │
                          └─────────────┬────────────────┘
                                       │ 패킷 단위 dispatch
                                       ▼
                          ┌────────────────────────────┐
                          │   JobQueue (per-Session)   │
                          │   ─────────────────────    │
                          │   같은 세션의 패킷은        │
                          │   순서가 보장된 채 직렬화   │
                          └────────────┬───────────────┘
                                       ▼
                          ┌────────────────────────────┐
                          │  JobDispatcher (Logic)     │
                          │   - 핸들러 등록 / 라우팅   │
                          └────────────────────────────┘
```

I/O는 Asio 워커가, 게임 로직은 별도 Logic 스레드(JobDispatcher)가 담당하는 분리 모델입니다.

상세 스레드 소유권과 수신·송신·종료 흐름은 [`docs/server-thread-architecture.html`](docs/server-thread-architecture.html)에서 확인할 수 있습니다.

---

## 빠른 시작

### 필요한 환경
- Windows 10/11 + Visual Studio 2022 (v143 toolset, C++20), 또는 Linux + GCC 14
- Boost 1.83 이상 (`include/boost` 로컬 복사본 또는 시스템 패키지)
- (선택) PostgreSQL — DB 계층 사용 시

### 빌드
```bash
cmake -S . -B build/linux -G Ninja \
  -DCMAKE_BUILD_TYPE=RelWithDebInfo -DBUILD_TESTING=ON
cmake --build build/linux --parallel
ctest --test-dir build/linux --output-on-failure
```

Windows 기존 개발 경로는 `MMOServer.sln`과 `DummyClient.sln`입니다. Linux/CMake 및
1,000명 장시간 검증은 [`BUILD_LINUX.md`](BUILD_LINUX.md)를 참고하세요.

### 실행
1. `PlayServer` 실행 → 7777 포트 listen
2. `ProtocolProbe`로 fragmentation/coalescing/비정상 frame preflight
3. 새 서버를 시작하고 `SoakClient`로 장시간 정상 패킷·재접속·ACK/RTT 검증

---

## 주요 기능

### 1. Boost.Asio 기반 비동기 네트워크 (`ServerCore/Network/`)

- `NetworkServer`: 기존 서버용 facade를 유지하면서 `io_context`와 워커 스레드 관리
- `NetworkSession`: 한 연결당 소켓 / 송수신 큐 / 세션 식별자
- `FrameDecoder`: `Peek`으로 길이 헤더를 확인하고 완성된 프레임만 꺼내 검증

Accept / Connect / Read / Write는 Asio 비동기 작업으로 처리합니다. 세션별 `strand`가 소켓과 송신 큐 접근을 직렬화하며, 수신 패킷은 기존 `JobQueue` / `JobDispatcher` 계층으로 전달됩니다.

**송수신 자료구조**
- 수신 버퍼: 세션별 **RingBuffer**의 최대 두 구간을 Asio에 직접 제공
- 송신 큐: `strand + deque<shared_ptr<NetworkPacket>>`, `post()` 전 예약을 포함한 세션별 대기 바이트 제한 적용
- 수신 JobQueue와 전체 세션 수도 기본 상한을 두며 `NetworkServer::Initialize()`에서 조정 가능

`NetworkServer::Shutdown()`은 앱의 control thread에서 호출해야 하며, Dispatcher보다 먼저 완료되어야 합니다.

---

### 2. Session 단위 직렬화 (Job 시스템)

같은 세션의 패킷이 여러 워커에 의해 동시 처리되면 게임 로직이 복잡해집니다. 이 프로젝트는 **세션별 JobQueue**로 이를 해결합니다:

1. 수신된 패킷은 세션의 JobQueue에 push
2. JobDispatcher가 그 JobQueue를 꺼내서 하나씩 핸들러 호출
3. 같은 세션의 패킷은 항상 단일 스레드에서 순서대로 처리됨

→ 핸들러 안에서는 락 없이 자유롭게 세션 상태를 만질 수 있습니다.

```cpp
// 핸들러 등록 예
class PlayServer : public BaseServerApp
{
    PlayServer()
    {
        RegisterPacket(&PlayServer::ON_CLGS_AUTHEN_REQ);
        // 더 등록...
    }

    void ON_CLGS_AUTHEN_REQ(int64_t sessionID, std::shared_ptr<CLGS_AUTHEN_REQT>& msg)
    {
        // 같은 sessionID의 핸들러는 직렬 호출 → 락 불필요
    }
};
```

---

### 3. 커스텀 메모리 풀 + STL Allocator

이 프로젝트에는 **TLS(Thread-Local Storage) 기반 메모리 풀**과 STL allocator 실험 코드가 들어 있습니다. 현재 활성 네트워크 경로는 수명 안전성과 Linux 이식성을 우선해 `std::make_shared`를 사용하며, 커스텀 풀은 Windows 벤치마크/실험 경로로 분리되어 있습니다.

**핵심 특징**
- **TLS 기반 chunk**: 같은 스레드의 alloc/free는 락 없이 처리
- **Lock-Free 전역 배치 스택**: chunk 부족 시 다른 스레드가 반납한 슬롯을 가져옴
- **Cross-thread free**: 다른 스레드가 할당한 슬롯을 안전하게 자기 풀로 회수
- **마크 기반 corruption 감지**: 슬롯마다 magic value 체크로 underflow / double-free 즉시 감지

**사용 방법**
```cpp
// 풀 적용을 명시적으로 선택한 실험 객체:
auto object = MakeMySharedPtr<MyPooledType>();

// 또는 STL 컨테이너에 적용:
std::vector<int, MyAllocator<int>> v;
```

`MakeMySharedPtr`는 내부적으로 `std::allocate_shared` + `MyAllocator<T>`를 사용해 컨트롤 블록까지 풀에서 할당합니다. Linux 서버 타깃에서는 128-bit CAS와 주소 배치 가정을 별도로 검증하기 전까지 이 경로를 제외합니다.

#### 벤치마크 결과

`make_shared` (기본 heap) vs `allocate_shared` (커스텀 풀) 비교 — 봇/패킷/세션 같은 객체 크기 대역에서 측정.

<details>
<summary><strong>벤치마크 보기</strong></summary>

**객체 크기 32B**
- 같은 스레드 alloc/free
  ![](MemoryPoolBenchmark/pad32_ST.png)
- 다른 스레드 간 (producer / consumer)
  ![](MemoryPoolBenchmark/pad32_CT.png)

**객체 크기 128B**
- SameThread
  ![](MemoryPoolBenchmark/pad128_ST.png)
- CrossThread
  ![](MemoryPoolBenchmark/pad128_CT.png)

**객체 크기 512B**
- SameThread
  ![](MemoryPoolBenchmark/pad512_ST.png)
- CrossThread
  ![](MemoryPoolBenchmark/pad512_CT.png)

**객체 크기 900B**
- SameThread
  ![](MemoryPoolBenchmark/pad900_ST.png)
- CrossThread
  ![](MemoryPoolBenchmark/pad900_CT.png)

</details>

같은 스레드 패턴에서는 풀이 일관되게 빠르고, cross-thread에서도 대부분의 사이즈 대역에서 우세를 유지합니다.

---

### 4. FlatBuffers 기반 프로토콜

- `flatbuffers/*.fbs`에 메시지 스키마 정의
- `build_protocol.bat`로 C++ 헤더 자동 생성
- `ThirdParty/FlatBuffers`에서 C# explicit union underlying type을 지원하는 커스텀
  `flatc` 소스를 MMOServer Git으로 직접 관리
- `MessageID` union으로 메시지 종류 enum화 → **컴파일 타임에 잘못된 핸들러 등록 차단**

커스텀 compiler, runtime header와 C++ 생성 코드는 다음 명령으로 같은 버전으로
빌드·동기화합니다. C# 생성 가능 여부도 함께 검사합니다.

```powershell
.\scripts\build_custom_flatbuffers.ps1
```

Windows CMake 구성에서는 `MMOCustomFlatc` target으로도 같은 작업을 실행할 수 있습니다.

```fbs
// 클라 → 게임 서버 요청 패턴
table CLGS_AUTHEN_REQ {
    seq: int;
    accounttoken: string;
    ...
}
```

생성된 `*T` 타입(C++ NativeTable)을 그대로 `Send()`에 넘기면 직렬화 + 송신까지 한 번에 처리됩니다.

---

### 5. PostgreSQL 연동 (libpqxx)

`ServerCore/DB/DBSession.h`에 row → struct 자동 바인딩 헬퍼가 있습니다 (POC 수준):

```cpp
pqxx::result result = ...;
std::vector<std::shared_ptr<Character>> characters;
Select(result, characters, [](auto obj) {
    return RowData{ obj->id, obj->name, obj->level };
});
```

> **참고**: 현재 connection pool, async query 같은 본격적인 DB 인프라는 아직 구현 전입니다.
> `db/DBSchema/` 디렉토리에 schema 정의와 Python 기반 migration 스크립트가 들어 있습니다.

---

## Linux 장시간 부하 테스트 도구

`SoakClient`는 기본 1,000개 연결을 ramp-up하고 봇당 초당 5개 요청을 보냅니다.
payload는 1~6,400바이트를 순환하며, 서버가 받은 payload의 FNV-1a checksum과 `seq`를
검사한 ACK만 성공으로 셉니다. RTT histogram, 누락/중복/오류 ACK, 재접속, CSV 통계를
기록합니다.

`ProtocolProbe`는 실제 `PlayServer`에 header/body fragmentation, 여러 frame
coalescing, zero/oversize/random/under-declared frame 및 10초 partial-frame deadline을
검증합니다. 예상 protocol error가 정상 soak 카운터를 오염시키지 않도록 disposable
preflight 서버에서 실행합니다.

```bash
# 기본: 1000명 × 5 PPS × 24시간, preflight/수집/판정 포함
bash scripts/run_soak.sh

# Docker/Compose 경로
bash scripts/run_docker_soak.sh

# 무제한: SIGINT/SIGTERM을 받을 때까지 실행
DURATION_SEC=0 bash scripts/run_docker_soak.sh
```

`DURATION_SEC=0`은 봇의 자동 종료 시각만 없앱니다. 직접 실행한 `SoakClient`는
`Ctrl+C` 또는 `SIGTERM`으로 정상 종료합니다. `run_docker_soak.sh`가 결과 분석까지
마치게 하려면 실행 중인 터미널을 끊는 대신 다른 터미널에서
`docker compose -f compose.soak.yml stop -t 30 loadbot`을 실행합니다. 봇이 진행 중인
요청의 ACK를 timeout까지 기다리고 최종 CSV 표본을 남긴 뒤 원래 스크립트가 서버
종료와 분석을 계속합니다.
실제 실행 시간이 24시간보다 짧으면 무제한 모드를 사용했더라도 24시간 합격으로
판정되지 않습니다. 무제한 실행에서는 CSV와 console/Compose 로그가 계속 커지므로
artifact 디스크 사용량도 함께 감시해야 합니다.

기존 `DummyClient/`는 Visual Studio용 학습/수동 시나리오 도구로 남아 있으며 Linux
24시간 합격 판정의 기준은 아닙니다.

---

## 디렉토리 구조

```
MMOServer/
├── ServerCore/                  ← 핵심 라이브러리 (정적 lib로 빌드)
│   ├── Network/                 ← Boost.Asio, Session, FrameDecoder, JobDispatcher
│   ├── Memory/                  ← MemoryPool, ObjectPool, RingBuffer, LockFree*
│   ├── Utill/                   ← Singleton, LogManager (spdlog 래핑), UID 등
│   ├── Dump/                    ← 미니덤프, 핸들러 후크
│   ├── DB/                      ← libpqxx 헬퍼
│   └── PCH/                     ← 프리컴파일 헤더
├── PlayServer/                  ← MMO 서버 본체 (애플리케이션)
├── SoakClient/                  ← Linux 장시간 봇 + raw protocol preflight
├── DummyClient/                 ← 기존 Visual Studio용 수동 봇
│   └── BotManager.*             ← 봇 시나리오 / 통계
├── scripts/                     ← soak 실행, 메트릭 판정, core 분석
├── Dockerfile / compose.soak.yml
├── flatbuffers/                 ← .fbs 스키마 + 자동 생성 헤더
├── db/DBSchema/                 ← PostgreSQL 스키마 + migration 스크립트
├── MemoryPoolBenchmark/         ← 벤치마크 결과 이미지
├── MMOServer.sln                ← PlayServer + ServerCore
└── DummyClient.sln              ← DummyClient + ServerCore
```

---

## 빌드 & 컨벤션

- **C++ 표준**: C++20
- **컴파일러**: MSVC v143 (Visual Studio 2022)
- **타깃**: x64 Debug / Release
- **네이밍**:
  - 클래스 / 메서드: `PascalCase`
  - 멤버 변수: `m_camelCase`
  - 정적 / 전역: `g_camelCase`
  - FlatBuffers 메시지: `CLGS_*_REQ` (클라→게임), `GSCL_NOTI_*` (서버→클라) 등 라우팅 prefix
- **중괄호**: Allman 스타일
- **로그**: `LOG_INFO("user=% level=%", id, lv)` — spdlog 래핑 + 자동 `[함수명 라인]` prefix
- **싱글톤**: `Singleton<T>` 상속 → `T::GetInstance()`
- **메모리**: 가능한 한 `MakeMySharedPtr<T>(...)` 사용 (커스텀 풀 우회 방지)

---

## 알려진 한계 / TODO

이 저장소는 진행 중인 학습 코드베이스라 아직 다음 항목들은 비어 있거나 단순화되어 있습니다:

- PlayerManager, RoomManager, AOI(관심영역) 등 **게임 로직 계층 미구현**
- DB ConnectionPool / async query 미구현 (현재는 헬퍼 함수만)
- TLS / 패킷 암호화: 현재는 고정 키 XOR 변환만 (운영용 보안 아님)
- 종료 경로 정리: 일부 Singleton은 process exit 시 OS가 정리

본 코드는 인프라 구현 / 검증에 집중되어 있으며, 게임 컨텐츠는 위 인프라가 안정화된 뒤 단계적으로 추가할 계획입니다.

---

## 라이선스 / 기여

개인 학습 프로젝트이며, 코드 리뷰 / 제안 / 이슈는 언제든지 환영합니다.
