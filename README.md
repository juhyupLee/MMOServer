# ⭐ MMO Server Project
고성능 C++ 기반 MMORPG 서버 아키텍처

---

## 🏗 Overview

이 프로젝트는 **대규모 동시 접속자 처리**를 목표로 설계된 C++ MMO 서버입니다.  
IOCP 기반 비동기 네트워크, FlatBuffers 프로토콜, PostgreSQL(libpqxx) 연동,  
커스텀 메모리 풀 allocator, 그리고 자동화된 DB Migration 시스템까지 포함하고 있어  
실제 상용 MMORPG 서버 구조를 학습하고 구현할 수 있습니다.

**핵심 구성 요소**

- IOCP 기반 네트워크 (RecvQ = RingBuffer, SendQ = Lock-Free Queue)
- Session → Logic Thread 모델
- FlatBuffers 기반 경량 프로토콜
- PostgreSQL(libpqxx) 트랜잭션 계층
- Thread-local Memory Pool 기반 STL Allocator
- PostgreSQL Schema 자동 생성 & Diff 적용 Python 툴

---

## 🚀 Features

---

### 📦 1. IOCP 기반 고성능 네트워크 시스템

**완전 비동기 Recv/Send**

- Windows IOCP 기반 Overlapped I/O 처리
- 소켓 이벤트 기반으로 초당 수십만 개 처리 가능

**버퍼 구조**

- **SendQ = Lock-Free Queue**
  - 네트워크 스레드와 로직 스레드 간 Lock contention 제거
  - 대량 송신 상황에서도 안정적인 성능 제공

**Session 단위 연결 관리**

- Session 객체 단위로 연결 / 패킷 / 상태 관리
- Disconnect 시 리소스 정리 및 안전한 세션 종료 처리

---

### 📦 2. Session → Logic Thread 모델

네트워크 스레드는 **I/O만 담당**하고,  
모든 게임 로직은 **전용 Logic Thread**에서 처리됩니다.

**장점**

- Race condition 최소화
- 스레드 락 의존도 감소
- 네트워크 코드와 게임 로직 코드의 책임 분리
- 유지보수 및 확장성 향상

→ 실제 대규모 MMORPG에서 사용되는 전형적인 서버 아키텍처 패턴입니다.

---

### 📦 3. FlatBuffers 기반 프로토콜

- 경량 Zero-copy 구조
- 직렬화/역직렬화 비용 최소화
- 명확한 스키마 기반 설계
- 서버–클라이언트 데이터 구조 일관성 유지
- 기존 FlatBuffers의 다소 불편한 사용성을 옵션 조정으로 개선

**사용 예시**

```cpp
auto packet = std::make_shared<CLGS_LoginReqT>();
packet->data1 = a;
packet->data2 = a;
// 이후 FlatBuffers 빌더를 통해 직렬화하여 전송
```
---

### 📦 4. PostgreSQL Schema 자동 생성 & Diff 적용 스크립트 (Python)

- Schema 파일로 DB 구조 정의
- 현재 DB와 비교하여 Diff 생성
- 변경사항만 자동 적용
- Migration 및 배포 관리 자동화
---

### 📦 5.Custom Memory Pool & STL Allocator
MMO 서버 최적화를 위해 제작된 고성능 메모리 풀 + Allocator

핵심 특징
- Thread-local Pool
    - 같은 스레드에서 할당/해제 시 거의 Overhead 없음
- Lock-Free Stack
    - malloc/free 대비 압도적인 속도
- Cross-thread Free 처리 지원
  - 스레드에서 free를 호출해도 안전하게 처리됨
- 중앙 Chunk Pool
    - 컬 풀이 부족할 경우 중앙 Pool에서 대여
- Debug Safety
  - Underflow/Overflow Marker
  - Memory Corruption 방지

- 성능 요약
  - SameThread 기준 최대 55% 향상 (ObjectPad=128)
  - CrossThread에서도 지속적으로 우세 (일부 2배 가까운 구간 존재)

### 📊 MemoryPool Benchmark

<details>
<summary><strong>📊 벤치마크 보기</strong></summary>

**ObjectPad = 32**
- SameThread  
  ![](MemoryPoolBenchmark/pad32_ST.png)
- CrossThread  
  ![](MemoryPoolBenchmark/pad32_CT.png)

---

**ObjectPad = 128**
- SameThread  
  ![](MemoryPoolBenchmark/pad128_ST.png)
- CrossThread  
  ![](MemoryPoolBenchmark/pad128_CT.png)

---

**ObjectPad = 512**
- SameThread  
  ![](MemoryPoolBenchmark/pad512_ST.png)
- CrossThread  
  ![](MemoryPoolBenchmark/pad512_CT.png)

---

**ObjectPad = 900**
- SameThread  
  ![](MemoryPoolBenchmark/pad900_ST.png)
- CrossThread  
  ![](MemoryPoolBenchmark/pad900_CT.png)

</details>

---
