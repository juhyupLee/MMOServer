# Custom C++ Memory Pool

🇰🇷 [한국어 문서 보기](README_KR.md)

---

## English Version

High-performance memory pool implementation designed for MMO server development.  
Supports thread-local allocation, lock-free free-lists, and cross-thread deallocation.

### ✨ Features
- **Thread-Local Storage (TLS)**: minimize contention with per-thread pools
- **Lock-Free Stack**: fast alloc/free without locks
- **Cross-Thread Free Handling**: efficiently manages frees from different threads
- **Central Pool Management**: threads borrow chunks from global pool when local is empty
- **Debug & Safety**: underflow/overflow markers, optional tracking

📊 Benchmark
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

🔮 Future Improvements

Hugepage alignment

Hazard pointers for lock-free reclamation


