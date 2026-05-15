# MMOServer

C++ / Windows IOCP 기반 MMO 서버 인프라와 부하 테스트 도구 모음입니다.
대규모 동시 접속을 가정한 네트워크 / 메모리 / 동기화 계층을 직접 구현해보는 것을 목표로 합니다.

> 이 저장소는 **인프라 학습 및 실험용 코드베이스**입니다.
> 실제 게임 컨텐츠 로직은 아직 최소한만 들어 있고, 네트워크 / 메모리 풀 / Lock-Free 자료구조 / 부하 테스트 같은 **서버 기반 시스템 구현**에 무게가 실려 있습니다.

---

## 한눈에 보는 구조

```
                          ┌────────────────────────────┐
   클라이언트 ─ TCP ─────▶ │ IOCP Worker Threads ( x5 ) │
                          │  - WSARecv / WSASend       │
                          │  - AcceptEx / ConnectEx    │
                          └────────────┬───────────────┘
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

I/O는 IOCP 워커가, 게임 로직은 별도 Logic 스레드(JobDispatcher)가 담당하는 전통적인 분리 모델입니다.

---

## 빠른 시작

### 필요한 환경
- Windows 10/11
- Visual Studio 2022 (v143 toolset, C++20)
- (선택) PostgreSQL — DB 계층 사용 시

### 빌드
```bash
# 서버 빌드
MMOServer.sln           # PlayServer + ServerCore

# 부하 테스트 클라이언트
DummyClient.sln         # DummyClient + ServerCore
```

각 솔루션을 Visual Studio에서 열거나, MSBuild로 빌드합니다.
산출물은 `bin/` 폴더에 `PlayServer_Debug.exe`, `DummyClient_Debug.exe` 등으로 생성됩니다.

### 실행
1. **PlayServer** 먼저 실행 → 7777 포트 listen
2. **DummyClient** 실행 → 콘솔 메뉴에서 봇 수 / 패킷 수 등 입력
3. 결과 확인 → 종료 시 SUMMARY 통계 출력

---

## 주요 기능

### 1. IOCP 기반 비동기 네트워크 (`ServerCore/Network/`)

- `NetworkServer`: IOCP 핸들 + 워커 스레드 풀 관리
- `NetworkSession`: 한 연결당 소켓 / 송수신 큐 / 세션 식별자
- `NetworkTask` (OVERLAPPED 상속): Accept / Connect / Recv / Send를 task로 추상화

각 비동기 IO 작업은 `NetworkTask` 파생 객체로 표현되고, IOCP completion 이벤트가 오면 워커가 `Run()`을 호출해 다음 단계를 실행합니다. 작업이 끝난 task는 워커가 자동으로 정리합니다.

**송수신 자료구조**
- 수신 버퍼: 세션별 **RingBuffer** (zero-copy로 WSARecv buffer 제공)
- 송신 큐: **Lock-Free Queue** (MS-queue 기반, 128-bit CAS로 ABA 방지)

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

`new`/`delete`나 `make_shared`를 그대로 쓰면 글로벌 heap에 락 경합이 생깁니다. 이 프로젝트는 **TLS(Thread-Local Storage) 기반 메모리 풀**을 직접 만들어 STL allocator와 통합합니다.

**핵심 특징**
- **TLS 기반 chunk**: 같은 스레드의 alloc/free는 락 없이 처리
- **Lock-Free 전역 배치 스택**: chunk 부족 시 다른 스레드가 반납한 슬롯을 가져옴
- **Cross-thread free**: 다른 스레드가 할당한 슬롯을 안전하게 자기 풀로 회수
- **마크 기반 corruption 감지**: 슬롯마다 magic value 체크로 underflow / double-free 즉시 감지

**사용 방법**
```cpp
// std::make_shared 대신:
auto session = MakeMySharedPtr<NetworkSession>(jobDispatcher);

// 또는 STL 컨테이너에 적용:
std::vector<int, MyAllocator<int>> v;
```

내부적으로 `std::allocate_shared` + `MyAllocator<T>`를 사용해 컨트롤 블록까지 풀에서 할당합니다.

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
- `MessageID` union으로 메시지 종류 enum화 → **컴파일 타임에 잘못된 핸들러 등록 차단**

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

## 부하 테스트 도구 (DummyClient)

서버를 정말 N명이 동시에 두드릴 때 잘 버티는지 검증하기 위한 **봇 클라이언트**입니다.
같은 ServerCore 라이브러리를 그대로 링크해서 사용합니다.

### 시나리오
| # | 시나리오 | 설명 |
|---|---|---|
| 1 | **Static** | N명 connect → 봇당 M개 패킷 송신 → idle |
| 2 | **Disconnect** | 송신 완료 후 봇 전원이 자발적으로 disconnect |
| 3 | **Reconnect** | 일부 봇이 끊고 → 일정 시간 후 재접속 → 재송신 |

### 실행 흐름
```
$ DummyClient_Debug.exe

=== DummyClient Bot Test ===
  1. Static send/recv
  2. Disconnect after sending
  3. Disconnect + reconnect (a fraction of bots)
Scenario (1-3) [1]: 3
Bot count [10]: 300
Packets per bot [30]: 30
Packet interval ms [100]: 50
Reconnect rate (%) [50]: 50
Reconnect delay ms [1000]: 500

[1sec] connected=300/0/0 sent=1820 disc=0 reco=0 err=0
[2sec] connected=295/0/5 sent=4500 disc=5 reco=2 err=0
...
=== SUMMARY ===
Elapsed: 7557 ms
Bots: 300
Total sent: 11250
Total disconnects: 150
Total reconnects: 75
Throughput: 1488 pkt/sec
```

봇 매니저가 1초마다 라이브 통계를, 종료 시 종합 요약을 출력합니다.

---

## 디렉토리 구조

```
MMOServer/
├── ServerCore/                  ← 핵심 라이브러리 (정적 lib로 빌드)
│   ├── Network/                 ← IOCP, Session, JobDispatcher, NetworkTask
│   ├── Memory/                  ← MemoryPool, ObjectPool, RingBuffer, LockFree*
│   ├── Utill/                   ← Singleton, LogManager (spdlog 래핑), UID 등
│   ├── Dump/                    ← 미니덤프, 핸들러 후크
│   ├── DB/                      ← libpqxx 헬퍼
│   └── PCH/                     ← 프리컴파일 헤더
├── PlayServer/                  ← MMO 서버 본체 (애플리케이션)
├── DummyClient/                 ← 봇 클라이언트 (부하 테스트)
│   └── BotManager.*             ← 봇 시나리오 / 통계
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
