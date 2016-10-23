#pragma once
//===================================================================================
//
// externals:

#include <mutex>
#include <atomic>
#include <assert.h>
#include <stdint.h>
#include <windows.h>

//===================================================================================
//
// publics:

#define BSR    _BitScanReverse
#define LOCK   std::mutex
#define INLINE __forceinline
#define SCOPE_LOCK(lock) ScopedLock var(&lock);
#define DELETE_CONSTRUCTOR_AND_DESTRUCTOR(classname) \
    classname() = delete; \
   ~classname() = delete; \

#define ATOMIC_VALUE(type) std::atomic<type>
#define THREAD_LOCAL(type) __declspec(thread) static type


static const size_t CBit = (size_t)1 << 0;
static const size_t PBit = (size_t)1 << 1;

/////////////////////////////////////////////////////////////////////////////////////

template <typename ret_t, typename mem_t, typename count_t>
INLINE ret_t add_mem(mem_t mem, count_t count)
{
	return reinterpret_cast<ret_t>(reinterpret_cast<char*>(mem) + count);
}


template <typename ret_t, typename mem_t, typename count_t>
INLINE ret_t sub_mem(mem_t mem, count_t count)
{
	return reinterpret_cast<ret_t>(reinterpret_cast<char*>(mem) - count);
}


//===================================================================================
//
// publics:

struct ScopedLock
{
	INLINE ScopedLock(const LOCK* lock)
	{
		if (m_lock)
			m_lock->lock();
	}

	INLINE ~ScopedLock()
	{
		if (m_lock)
			m_lock->unlock();
	}

	LOCK* m_lock;
};
