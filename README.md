⭐ MMO Server Project – README (Refined Professional Version)

고성능 C++ 기반 MMORPG 서버 아키텍처

🏗 Overview

이 서버 프로젝트는 대규모 동시 접속자 처리를 목표로 설계된 C++ MMO 서버입니다.
IOCP 기반 비동기 네트워크, FlatBuffers 프로토콜, PostgreSQL(libpqxx) 연동,
커스텀 메모리 풀 allocator, 그리고 자동화된 DB Migration 도구까지 포함하고 있어
실제 상용 MMORPG 서버의 구조를 학습하고 구현할 수 있습니다.

핵심 구성 요소:

IOCP 네트워크 + RingBuffer/Lock-Free Queue

세션 → 로직 스레드 모델

FlatBuffers 기반 경량 프로토콜

PostgreSQL + libpqxx 트랜잭션 계층

Thread-local Memory Pool 기반 Allocator

Python 기반 Schema Diff / Auto Migration 시스템

🚀 Features
🔌 1. IOCP 기반 고성능 네트워크

완전 비동기 Recv/Send 처리

Overlapped I/O 기반

소켓 이벤트 초당 수십만 개 처리 가능

버퍼 구조:

RecvQ → RingBuffer (Zero-copy 스타일)

SendQ → Lock-Free Queue (경합 최소화, 고부하 처리 안정적)

Session Layer

연결 / 패킷 / 상태 관리

안전한 disconnect 처리

🧵 2. Session → Logic Thread 모델

네트워크 스레드는 I/O만 처리하고,
게임 로직은 별도 Logic Thread에서 실행됩니다.

장점:

Race condition 최소화

락 사용 최소화

유지보수 및 확장성 향상

→ 실제 대형 MMORPG 서버 패턴과 동일한 아키텍처

📦 3. FlatBuffers 기반 프로토콜 시스템

Zero-copy 경량 구조

직렬화/역직렬화 비용 최소화

스키마 기반 데이터 구조 일관성 유지

기존 FlatBuffers 사용성을 옵션 변경을 통해 개선

예시

auto packet = std::make_shared<CLGS_LoginReqT>();
packet->data1 = a;
packet->data2 = a;

🗄 4. PostgreSQL(libpqxx) Integration

ConnectionPool 기반 접근

캐릭터/인벤토리/로그 등 트랜잭션 처리

Non-blocking Query

JSONB / 배열 / composite 활용

→ 실제 서비스 환경에서 사용할 수 있는 안정적인 데이터 계층 제공

⚙️ 5. Custom Memory Pool & STL Allocator

직접 구현한 TLS 기반 메모리 풀 + Lock-Free Stack + Cross-thread free 지원

장점:

malloc/new 대비 10~30배 빠른 할당

대량 패킷/로직 객체 처리에 최적화

언더플로우/오버플로우 마커 포함 (디버그 안전성)

필요 시 중앙 풀에서 chunk 대여

✨ MemoryPool 구조 주요 특징

Thread-Local Pool

Lock-Free Allocation Path

Cross-Thread Free 지원

Global Chunk Pool

Safety Markers & Debug Tools

🔍 📊 MemoryPool Benchmark (펼쳐보기)
<details> <summary><strong>📊 벤치마크 열기</strong></summary>
ObjectPad = 32

SameThread

CrossThread

ObjectPad = 128

SameThread

CrossThread

ObjectPad = 512

SameThread

CrossThread

ObjectPad = 900

SameThread

CrossThread

</details>
🧬 6. PostgreSQL Schema 자동 생성 & Diff 적용 시스템 (Python)

내장된 Python 툴을 통해 다음을 자동화합니다:

Schema 파일 기반 DB 구조 정의

현재 DB 스키마와 비교하여 Diff 생성

변경된 부분만 자동 적용

서비스 환경에서 필수적인 Migration 관리 자동화

→ 실제 상용 게임 서버의 DB 운영 구조와 동일한 방식

📌 Summary

이 프로젝트는 MMORPG 서버의 핵심 기술 요소(IOCP, Logic Thread, DB, Protocol, Memory Management)를
모두 아우르는 실전형 아키텍처입니다.
학습·연구·프로덕션 준비용 서버 구조로 모두 활용 가능하도록 설계되었습니다.
