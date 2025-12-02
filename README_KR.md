⭐ MMO Server Project – README (Professional Version)

고성능 C++ 기반 MMORPG 서버 아키텍처

🏗 Overview

이 프로젝트는 대규모 동시 접속자 처리를 목표로 설계된 C++ MMO 서버로,
IOCP 기반 비동기 네트워크, FlatBuffers 프로토콜, PostgreSQL 트랜잭션 처리,
그리고 커스텀 메모리 풀 allocator를 중심으로 구성되어 있습니다.

특히 SendQ는 Lock-Free 큐로 구현되어 있어
스레드 경합을 최소화하고, 네트워크 처리 비용을 크게 절감합니다.

또한 PostgreSQL 스키마를 자동 생성하고 Diff 적용까지 처리하는
**DB Migration 자동화 시스템(Python)**이 내장되어 있어 실제 서비스 환경에서도 확장 가능합니다.

🚀 Features
🔌 1. IOCP 기반 고성능 네트워크 시스템
✔ 완전 비동기 Recv/Send

Windows IOCP 기반 Overlapped I/O 처리

소켓 이벤트 기반으로 초당 수십만 개 처리 가능

✔ SendQ = Lock-Free Queue

네트워크 스레드와 로직 스레드 간 Lock contention 제거

대량 송신 상황에서도 안정적인 성능 제공

✔ Session 단위 연결 관리

Session 객체로 연결·패킷·상태 관리

Disconnect 시 안전한 자원 회수

🧵 2. Session → Logic Thread 모델

네트워크 스레드는 I/O만 담당하고,
모든 게임 로직은 전용 Logic Thread에서 처리됩니다.

장점:

Race condition 최소화

스레드 락 의존도 제거

코드 구조 분리로 유지보수성 증가

실제 대규모 MMORPG에서 사용하는 패턴과 동일합니다.

📦 3. FlatBuffers 기반 프로토콜

경량 Zero-copy 구조

직렬화/역직렬화 비용 최소

명확한 스키마 기반 설계

서버–클라이언트 데이터 구조 일관성 유지

기존 flatbuffuer의 불편한 사용성을 옵션을 변경하여 사용성을 개선함

실제 사용 예:
auto packet = std::make_shared<CLGS_LoginReqT>();
packet->data1 = a;
packet->data2 = a;

🗄 4. PostgreSQL(libpqxx) 통합

ConnectionPool 기반 DB 접근

트랜잭션 기반 캐릭터·인벤토리·로그 처리

Non-blocking Query 가능

JSONB, 배열, 복합 타입 활용 능력 내장

서비스 환경에서도 견고한 데이터 계층을 구성할 수 있습니다.

⚙️ 5. Custom Memory Pool & STL Allocator

MMO 서버 개발을 위해 설계된 고성능 메모리 풀 구현체입니다.  

직접 구현한 Thread-local Memory Pool + Pool Allocator 내장:

소규모 객체할당 최적화

malloc/new 대비 10~30배 빠름

고빈도 패킷·로직 객체에 최적화

스레드 로컬 할당, 락프리 구조, 크로스 스레드 해제를 지원합니다.

### ✨ 특징
- **TLS 기반 메모리 풀**: 스레드 단위 풀 관리 → 경쟁 최소화
- **락프리 스택**: 빠른 할당/해제, 락 없이 처리
- **크로스 스레드 Free 처리**: 다른 스레드에서 해제되는 경우에도 효율적 처리
- **중앙 풀 관리**: 로컬 풀이 부족하면 글로벌 풀에서 청크를 빌려 사용
- **디버그 및 안전성**: 언더플로우/오버플로우 마커, 선택적 메모리 추적 기능

---

<details>
<summary><strong>📊 상세 벤치마크 보기</strong></summary>

### 📊 벤치마크
#### ObjectPad = 32
- SameThread  
  ![Pad32 ST](MemoryPoolBenchmark/pad32_ST.png)
- CrossThread  
  ![Pad32 CT](MemoryPoolBenchmark/pad32_CT.png)

#### ObjectPad = 128
- SameThread  
  ![Pad128 ST](MemoryPoolBenchmark/pad128_ST.png)
- CrossThread  
  ![Pad128 CT](MemoryPoolBenchmark/pad128_CT.png)

#### ObjectPad = 512
- SameThread  
  ![Pad512 ST](MemoryPoolBenchmark/pad512_ST.png)
- CrossThread  
  ![Pad512 CT](MemoryPoolBenchmark/pad512_CT.png)

#### ObjectPad = 900
- SameThread  
  ![Pad900 ST](MemoryPoolBenchmark/pad900_ST.png)
- CrossThread  
  ![Pad900 CT](MemoryPoolBenchmark/pad900_CT.png)

---

🧬 6. PostgreSQL Schema 자동 생성 & Diff 적용 스크립트 (Python)

내장된 Python 툴로 다음 기능을 수행합니다:

✔ Schema 파일로 DB 구조 정의
✔ 현재 DB와 비교하여 Diff 생성
✔ 변경사항만 자동 적용
✔ Migration 관리 자동화

실제 운영 게임에서 DB 스키마 버전 관리에 필수적인 기능입니다.

