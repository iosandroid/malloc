//===================================================================================
//
//

#include <atomic>
#include <stdint.h>
#include <assert.h>
#include <windows.h>
//#include "malloc.c.h"

#define thread_local(type) __declspec(thread) static type

class m_allocator
{
public:
	static void* m_alloc(size_t size)
	{
		if (m_map.thread_index == 0)
		{
			m_map.thread_index = ++m_map.thread_count;
			assert(m_map.thread_index < MaxThreadCount);
		}

		p_thread_local_pool& pool = m_map.thread_pool[m_map.thread_index];
		if (!pool)
		{
			pool = m_thread_local_init_pool();
		}

		return m_thread_local_alloc(m_map.thread_pool[m_map.thread_index], size);
	}

	static void m_free(void* p)
	{
		if (m_map.thread_index == 0)
		{
			m_map.thread_index = ++m_map.thread_count;
			assert(m_map.thread_index < MaxThreadCount);
		}

		p_thread_local_pool pool = m_map.thread_pool[m_map.thread_index];
		if (pool)
		{
			m_thread_local_free(pool, p);
		}
	}

private:
	using atomic_value = std::atomic < size_t >;

	static const size_t InUseBit       = 0x01;
	static const size_t ListBinCount   = 0x20;
	static const size_t MaxThreadCount = 0x40;

private:

	/////////////////////////////////////////////////////////////////////////////////
	struct m_ctrl_block;
	using  p_ctrl_block = m_ctrl_block*;

	struct m_ctrl_block
	{
		size_t prev;
		size_t head;
	};
	
	/////////////////////////////////////////////////////////////////////////////////
	struct m_list_block;
	using  p_list_block = m_list_block*;

	struct m_list_block : public m_ctrl_block
	{
		p_list_block next;
		p_list_block prev;
	};
	
	/////////////////////////////////////////////////////////////////////////////////
	struct m_tree_block;
	using  p_tree_block = m_tree_block*;

	struct m_tree_block : public m_ctrl_block
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
		p_ctrl_block head;

		uint32_t     listmaps;
		m_list_block listbins[ListBinCount];
	};

	/////////////////////////////////////////////////////////////////////////////////
	struct m_thread_pool_map
	{
		atomic_value         thread_count;
		thread_local(size_t) thread_index;

		p_thread_local_pool  thread_pool[MaxThreadCount];
	};
	
private:
	static const size_t OneChunkSize = sizeof(m_ctrl_block);
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

			pool->head = add_blk(pool, sizeof(m_thread_local_pool));
			set_size(pool->head, size - sizeof(m_thread_local_pool));

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

	static void* m_thread_local_alloc(p_thread_local_pool pool, size_t requested)
	{
		void* mem = NULL;		
		if (requested < MaxListBlockRequest)
		{
			size_t size = (requested < MinRequestSize) ? OneChunkSize : (requested + OneChunkSize + 0x7) & ~0x7;
			if (size < get_size(pool->head))
			{
				size_t rest = get_size(pool->head) - size;

				p_ctrl_block p = pool->head;

				set_size(p, size);
				set_inuse(p);

				pool->head = add_blk(p, size);
				set_size(pool->head, rest);

				mem = blk_to_mem(p);
			}
		}

		return mem;
	}

	static void m_thread_local_free(p_thread_local_pool pool, void* mem)
	{
		p_ctrl_block curr = mem_to_blk(mem);
		size_t size = get_size(curr);

		p_ctrl_block next = add_blk(curr, size);
		if (!get_inuse(next))
		{

		}
	}

private:
	inline static size_t get_size(p_ctrl_block blk)
	{
		return blk->head & ~InUseBit;
	}

	inline static void set_size(p_ctrl_block blk, size_t size)
	{
		blk->head = size & ~InUseBit;
	}

	inline static bool get_inuse(p_ctrl_block blk)
	{
		return blk->head & InUseBit;
	}

	inline static bool set_inuse(p_ctrl_block blk)
	{
		return blk->head | InUseBit;
	}

	inline static void* blk_to_mem(p_ctrl_block p)
	{
		return reinterpret_cast<char*>(p) + OneChunkSize;
	}

	inline static p_ctrl_block mem_to_blk(void* mem)
	{
		return sub_blk(mem, OneChunkSize);
	}

	template <typename ret_t, typename mem_t, typename count_t> 
	inline static ret_t add_mem(mem_t mem, count_t count)
	{
		return reinterpret_cast<ret_t>(reinterpret_cast<char*>(mem) + count);
	}

	template <typename mem_t, typename count_t> 
	inline static p_ctrl_block add_blk(mem_t mem, count_t count)
	{
		return add_mem<p_ctrl_block>(mem, count);
	}

	template <typename ret_t, typename mem_t, typename count_t>
	inline static ret_t sub_mem(mem_t mem, count_t count)
	{
		return reinterpret_cast<ret_t>(reinterpret_cast<char*>(mem) - count);
	}

	template <typename mem_t, typename count_t>
	inline static p_ctrl_block sub_blk(mem_t mem, count_t count)
	{
		return sub_mem<p_ctrl_block>(mem, count);
	}

private:
	static m_thread_pool_map m_map;	
};

m_allocator::m_thread_pool_map m_allocator::m_map;
size_t m_allocator::m_thread_pool_map::thread_index = 0;


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