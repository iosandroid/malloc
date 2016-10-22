//===================================================================================
//
// externals:

#include <atomic>
#include <stdint.h>

//===================================================================================
//
// publicL

#define ATOMIC_VALUE(type) std::atomic<type>
#define THREAD_LOCAL(type) __declspec(thread) static type

/////////////////////////////////////////////////////////////////////////////////////

class Allocator
{
public:
	Allocator(size_t thread_local_capacity = 0);
	virtual ~Allocator();

	void* malloc(size_t size);
	void  free(void* p);

private:
	enum 
	{
		MaxThreadCount = 0x40
	};

	using p_pool_local = struct m_pool_local*;
	p_pool_local m_ThreadPool[MaxThreadCount];

private:
	p_pool_local pool_construct(size_t capacity = 0);
	void         pool_destruct(p_pool_local pool);

private:
	ATOMIC_VALUE(uint16_t) m_ThreadCount;
	THREAD_LOCAL(uint16_t) m_ThreadIndex;

	size_t                 m_ThreadLocalCapacity;
};
