
## 한국어 버전

MMO 서버 개발을 위해 설계된 고성능 메모리 풀 구현체입니다.  
스레드 로컬 할당, 락프리 구조, 크로스 스레드 해제를 지원합니다.

### ✨ 특징
- **TLS 기반 메모리 풀**: 스레드 단위 풀 관리 → 경쟁 최소화
- **락프리 스택**: 빠른 할당/해제, 락 없이 처리
- **크로스 스레드 Free 처리**: 다른 스레드에서 해제되는 경우에도 효율적 처리
- **중앙 풀 관리**: 로컬 풀이 부족하면 글로벌 풀에서 청크를 빌려 사용
- **디버그 및 안전성**: 언더플로우/오버플로우 마커, 선택적 메모리 추적 기능

---

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
