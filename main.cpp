//===================================================================================
//
//

#include <stdint.h>
#include "malloc.c.h"

#define thread_local(type) __declspec(thread) static type

class m_allocator
{
public:
	static void* m_alloc(size_t size)
	{
		if (m_pool == 0)
			m_pool = m_thread_local_init_pool();

		return m_thread_local_alloc(m_pool, size);
	}

	static void m_free(void* p)
	{

	}

private:
	static const size_t InUseBit     = 0x01;
	static const size_t ListBinCount = 0x20;

private:

	/////////////////////////////////////////////////////////////////////////////////
	struct m_head_block;
	using  p_head_block = m_head_block*;

	struct m_head_block
	{
		size_t reserved;
	};
	
	/////////////////////////////////////////////////////////////////////////////////
	struct m_list_block;
	using p_list_block = m_list_block*;

	struct m_list_block : public m_head_block
	{
		p_list_block next;
		p_list_block prev;
	};
	
	/////////////////////////////////////////////////////////////////////////////////
	struct m_tree_block;
	using  p_tree_block = m_tree_block*;

	struct m_tree_block : public m_head_block
	{
		p_tree_block left;
		p_tree_block right;

		p_tree_block parent;
	};	
	
	/////////////////////////////////////////////////////////////////////////////////
	struct m_thread_local_pool;
	using  p_thread_local_pool = m_thread_local_pool*;

	struct m_thread_local_pool
	{
		p_head_block head;

		uint32_t     listmaps;
		m_list_block listbins[ListBinCount];
	};
	
private:
	static const size_t OneChunkSize = sizeof(m_head_block);
	static const size_t MinChunkSize = (OneChunkSize + 0x7) & ~0x7;
	
	static const size_t MinRequestSize = MinChunkSize - OneChunkSize;
	static const size_t MaxRequestSize = (-MinChunkSize) << 2;

	static const size_t PowerOfTwo = 10;
	static const size_t MinTreeBlockSize = 1 << PowerOfTwo;
	static const size_t MaxListBlockSize = MinTreeBlockSize - 1;
	static const size_t MaxListBlockRequest = MaxListBlockSize - OneChunkSize - 0x7;

private:
	static p_thread_local_pool m_thread_local_init_pool(size_t capacity = 0)
	{
		p_thread_local_pool pool = NULL;

		SYSTEM_INFO info;
		::GetSystemInfo(&info);

		size_t gran = info.dwAllocationGranularity;
		size_t size = (capacity + (gran << 1) - 1) & ~(gran - 1); // align capacity to granularity size

		void* memory = ::VirtualAlloc(0, size, MEM_RESERVE | MEM_COMMIT, PAGE_READWRITE);
		if (memory)
		{
			::ZeroMemory(memory, size);

			pool = static_cast<p_thread_local_pool>(memory);
			pool->listmaps = 0;

			pool->head = reinterpret_cast<p_head_block>(&pool + sizeof(m_thread_local_pool));
			pool->head->reserved = size - sizeof(m_thread_local_pool);

			// init list bins
			for (size_t i = 0; i < ListBinCount; i++)
			{
				pool->listbins[i].next = pool->listbins[i].prev = &pool->listbins[i];
			}
		}

		return pool;
	}

	static void m_thread_local_fini_pool(p_thread_local_pool pool)
	{
	}

	static void* m_thread_local_alloc(p_thread_local_pool pool, size_t size)
	{
		void* mem = NULL;		
		if (size < MaxListBlockRequest)
		{
			size_t request = (size < MinRequestSize) ? OneChunkSize : (size + OneChunkSize + 0x7) & ~0x7;
			if (request < pool->head->reserved)
			{
				size_t residual_size = pool->head->reserved - request;

				p_head_block p = pool->head;
				p->reserved = request | InUseBit;

				pool->head = reinterpret_cast<p_head_block>(reinterpret_cast<char*>(p) + request);
				pool->head->reserved = residual_size;

				mem = reinterpret_cast<char*>(p) + OneChunkSize;
			}
		}

		return mem;
	}

	static void m_thread_local_free(p_thread_local_pool pool, void* p)
	{

	}

private:
	thread_local(p_thread_local_pool) m_pool;
};

m_allocator::p_thread_local_pool m_allocator::m_pool = NULL;


/////////////////////////////////////////////////////////////////////////////////////
void main()
{
	void* p = m_allocator::m_alloc(17);

	//mspace msp = create_mspace(256, 0);

	//const size_t Count = 16;
	//void* p[Count] = {NULL};

	//for (size_t i = 0; i < Count; i++)
	//{
	//	p[i] = mspace_malloc(msp, 16);
	//}
	//
	//for (size_t i = 0; i < 8; i++)
	//{
	//	mspace_free(msp, p[i]);
	//}
	//
	//void* p0 = mspace_malloc(msp, 16);

	return;
}