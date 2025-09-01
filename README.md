# Custom C++ Memory Pool

🌐 [English](#english-version) | 🇰🇷 [한국어](#한국어-버전)

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

### 🚀 Usage Example
```cpp
#include "MemoryPool.h"

struct Player { int id; char name[32]; };

int main()
{
    MemoryPool<Player> pool;
    pool.ChunkInit(1000);

    Player* p = pool.Alloc();
    p->id = 1;

    pool.Free(p);
    return 0;
}
📊 Benchmark
Scenario	Alloc Count	Time (ms)	TPS
new/delete	8,000,000	98432	~8.5 M/s
MemoryPool Alloc/Free	8,000,000	26586	~30 M/s

🔮 Future Improvements

Hugepage alignment

Hazard pointers for lock-free reclamation

