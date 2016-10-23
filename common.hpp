#pragma once
//===================================================================================
// 
// Brief description:
//
// Have implemented two classes: SmallBlockAllocator and LargeBlockAllocator. 
// They have a very similar design and independent from each other. SmallBlockAllocator intended to manage memory pool
// only of small blocks ( < 256 bytes). LargeBlockAllocator can manage memory pool of small blocks
// and large blocks, but SmallBlockAllocator manages small blocks more effectively.
// It could be done not very big effort to unite those two classes in the one universal
// allocator, but i didn't catch with the time, so i hope i could present the result as it is at this point.
//
// As mentioned before they have a very similar design. The difference is in the internal
// data structures used for allocated blocks. 
//
// SmallBlockAllocator uses inside an array of
// doubly linked lists to store freed blocks of memory for future use. It is rather efficient
// for not very large blocks, because 1) the blocks could be added and retrieved from such
// data structure in O(1) time; 2) the "service" memory that is needed to keep such data structure
// is smaller. 
//
// LargeBlockAllocator uses inside an array of binary trees to store blocks of memory for future use.
// Each tree in the arrays stores blocks of memory of the specified size. It is of course less efficient
// than the first case (~ O(log(n)) time) and requires more "service" memory, but it allows to handle 
// small and large blocks at the same time.
//
// Both implementations use the same design for multithreaded environment. Allocator constructs inside number of
// thread local pools which is about the same as the number of threads in the system. When some thread requests
// the memory allocator runs through circular list of memory pools trying to lock one. If it succeed it uses
// those memory pool to allocate the memory block. When some thread trying to free memory block, allocator
// using the "service" information which is stored in that memory block to determine to which memory pool
// those block belongs, and then locks that pool to free the memory block.

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
