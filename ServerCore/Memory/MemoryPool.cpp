
#include "MemoryPool.h"
MemoryPool gMemoryPool;
MemoryPool::MemoryPool()
{
	int32_t memorySize = 1;
	auto memoryPool64 = new ObjectPool<Buffer<64>>();
	for (memorySize; memorySize <= 64; ++memorySize)
	{
		m_memory[memorySize] = memoryPool64;
	}

	auto memoryPool128 = new ObjectPool<Buffer<128>>();
	for (memorySize; memorySize <= 128; ++memorySize)
	{
		m_memory[memorySize] = memoryPool128;
	}

	auto memoryPool256 = new ObjectPool<Buffer<256>>();
	for (memorySize; memorySize <= 256; ++memorySize)
	{
		m_memory[memorySize] = memoryPool256;
	}

	auto memoryPool512 = new ObjectPool<Buffer<512>>();
	for (memorySize; memorySize <= 512; ++memorySize)
	{
		m_memory[memorySize] = memoryPool512;
	}

	auto memoryPool1024 = new ObjectPool<Buffer<1024>>();
	for (memorySize; memorySize <= 1024; ++memorySize)
	{
		m_memory[memorySize] = memoryPool1024;
	}

	auto memoryPool2048 = new ObjectPool<Buffer<2048>>();
	for (memorySize; memorySize <= 2048; ++memorySize)
	{
		m_memory[memorySize] = memoryPool2048;
	}

	auto memoryPool4096 = new ObjectPool<Buffer<4096>>();
	for (memorySize; memorySize <= 4096; ++memorySize)
	{
		m_memory[memorySize] = memoryPool4096;
	}
}

void* MemoryPool::Alloc(size_t size)
{
	if(size > PAGE_SIZE)
	{
		return AlignedMalloc(size, 16);
	}
	else
	{
		return m_memory[size]->AllocFromChunk(size);
	}
}

void MemoryPool::Free(void* ptr)
{
	auto metadata = reinterpret_cast<ChunkMetadata*>(static_cast<char*>(ptr) - sizeof(ChunkMetadata));
	if (metadata->m_size > PAGE_SIZE)
	{
		AlignedFree(ptr);
	}
	else
	{
		m_memory[metadata->m_size]->FreeToChunk(ptr);
	}
}
