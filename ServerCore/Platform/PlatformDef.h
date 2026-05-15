#pragma once

// ============================================================
// Cross-platform abstractions for Windows-specific APIs
// ============================================================

#ifdef _WIN32
	#define WIN32_LEAN_AND_MEAN
	#define NOMINMAX
	#include <Windows.h>
	#include <intrin.h>
#else
	#include <cstdlib>
	#include <cstring>
	#include <csignal>
	#include <unistd.h>
#endif

#include <cstdint>
#include <atomic>
#include <thread>
#include <chrono>
#include <new>

// ============================================================
// Aligned Memory Allocation
// ============================================================
inline void* AlignedMalloc(size_t size, size_t alignment)
{
#ifdef _WIN32
	return _aligned_malloc(size, alignment);
#else
	// std::aligned_alloc requires size to be a multiple of alignment
	size_t aligned_size = (size + alignment - 1) & ~(alignment - 1);
	return std::aligned_alloc(alignment, aligned_size);
#endif
}

inline void AlignedFree(void* ptr)
{
#ifdef _WIN32
	_aligned_free(ptr);
#else
	std::free(ptr);
#endif
}

// ============================================================
// 128-bit Compare And Swap (Double-Width CAS)
// ============================================================
#ifdef _WIN32

inline bool AtomicCompareExchange128(
	volatile int64_t* dest,
	int64_t exchangeHigh,
	int64_t exchangeLow,
	int64_t* comparand)
{
	return InterlockedCompareExchange128(dest, exchangeHigh, exchangeLow, comparand) != 0;
}

#else

inline bool AtomicCompareExchange128(
	volatile int64_t* dest,
	int64_t exchangeHigh,
	int64_t exchangeLow,
	int64_t* comparand)
{
	// GCC/Clang __int128 based CAS
	using uint128_t = unsigned __int128;

	uint128_t expected_val =
		(static_cast<uint128_t>(static_cast<uint64_t>(comparand[1])) << 64) |
		static_cast<uint64_t>(comparand[0]);

	uint128_t desired_val =
		(static_cast<uint128_t>(static_cast<uint64_t>(exchangeHigh)) << 64) |
		static_cast<uint64_t>(exchangeLow);

	bool result = __atomic_compare_exchange_n(
		reinterpret_cast<volatile uint128_t*>(dest),
		&expected_val,
		desired_val,
		false,
		__ATOMIC_SEQ_CST,
		__ATOMIC_SEQ_CST);

	if (!result)
	{
		comparand[0] = static_cast<int64_t>(expected_val);
		comparand[1] = static_cast<int64_t>(expected_val >> 64);
	}
	return result;
}

#endif

// ============================================================
// Atomic Increment / Decrement / Exchange / CAS (32-bit, 64-bit)
// ============================================================
#ifdef _WIN32

inline int32_t AtomicIncrement32(volatile long* target)
{
	return InterlockedIncrement(target);
}

inline int32_t AtomicDecrement32(volatile long* target)
{
	return InterlockedDecrement(target);
}

inline int64_t AtomicIncrement64(volatile int64_t* target)
{
	return InterlockedIncrement64(target);
}

inline int64_t AtomicCAS64(volatile int64_t* dest, int64_t exchange, int64_t comparand)
{
	return InterlockedCompareExchange64(dest, exchange, comparand);
}

#else

inline int32_t AtomicIncrement32(volatile int32_t* target)
{
	return __atomic_add_fetch(target, 1, __ATOMIC_SEQ_CST);
}

inline int32_t AtomicDecrement32(volatile int32_t* target)
{
	return __atomic_sub_fetch(target, 1, __ATOMIC_SEQ_CST);
}

inline int64_t AtomicIncrement64(volatile int64_t* target)
{
	return __atomic_add_fetch(target, 1, __ATOMIC_SEQ_CST);
}

inline int64_t AtomicCAS64(volatile int64_t* dest, int64_t exchange, int64_t comparand)
{
	__atomic_compare_exchange_n(dest, &comparand, exchange, false,
		__ATOMIC_SEQ_CST, __ATOMIC_SEQ_CST);
	return comparand;
}

#endif

// ============================================================
// YieldProcessor
// ============================================================
#ifndef _WIN32
	#if defined(__x86_64__) || defined(__i386__)
		inline void YieldProcessor() { __asm__ __volatile__("pause"); }
	#elif defined(__aarch64__)
		inline void YieldProcessor() { __asm__ __volatile__("yield"); }
	#else
		inline void YieldProcessor() { std::this_thread::yield(); }
	#endif
#endif

// ============================================================
// ZeroMemory
// ============================================================
#ifndef _WIN32
	#define ZeroMemory(dest, size) std::memset((dest), 0, (size))
#endif

// ============================================================
// Type Compatibility (LONG, DWORD, BOOL, BYTE, etc.)
// ============================================================
#ifndef _WIN32
	using LONG = volatile int32_t;
	using DWORD = uint32_t;
	using BOOL = int;
	using BYTE = uint8_t;

	#ifndef TRUE
		#define TRUE 1
	#endif
	#ifndef FALSE
		#define FALSE 0
	#endif
	#ifndef INVALID_HANDLE_VALUE
		#define INVALID_HANDLE_VALUE ((void*)(intptr_t)-1)
	#endif

	using HANDLE = void*;
	using LONG64 = int64_t;
	using ULONG_PTR = uintptr_t;
#endif

// ============================================================
// wchar_t utilities
// ============================================================
#ifndef _WIN32
	#include <cwchar>
	inline void wcscpy_s(wchar_t* dest, size_t destSize, const wchar_t* src)
	{
		wcsncpy(dest, src, destSize - 1);
		dest[destSize - 1] = L'\0';
	}
	inline void wcscpy_s(wchar_t* dest, const wchar_t* src)
	{
		wcscpy(dest, src);
	}
#endif
