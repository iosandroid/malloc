//===================================================================================
//
// externals:

#include <atomic>
#include <stdint.h>

//===================================================================================
//
// public:

#define ATOMIC_VALUE(type) std::atomic<type>
#define THREAD_LOCAL(type) __declspec(thread) static type

/////////////////////////////////////////////////////////////////////////////////////

class LargeBlockAllocator
{
public:
	LargeBlockAllocator(size_t thread_local_capacity = 0);
	virtual ~LargeBlockAllocator();

	void* malloc(size_t size);
	void  free(void* umem);

private:
	enum 
	{
		MaxThreadCount = 0x10
	};

	using p_pool_local = struct m_pool_local*;
	p_pool_local m_ThreadPool[MaxThreadCount];

private:
	p_pool_local pool_construct(size_t capacity);
	void         pool_destruct(p_pool_local pool);

private:
	ATOMIC_VALUE(uint16_t) m_ThreadCount;
	THREAD_LOCAL(uint16_t) m_ThreadIndex;
};
