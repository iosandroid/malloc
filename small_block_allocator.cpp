//===================================================================================
//
// externals:

#include "small_block_allocator.hpp"

//===================================================================================
//
// forward decls:

namespace Small
{
	struct m_ctrl_block;
	struct m_pool_local;

	using  p_ctrl_block = m_ctrl_block*;
	using  p_pool_local = m_pool_local*;

}; //namespace Small

//===================================================================================
//
// publics:

namespace Small
{
	// This is a data structure which is used as a "service" header for user memory
	// block. 
	struct m_ctrl_block
	{
		size_t       m_head; // stores the size of the previos memory block
		size_t       m_data; // stores the size of the current memory block + 
							 // 2 less significant bits used as flags: whether
							 // the current block and the previous block are in use

		p_pool_local m_pool; // parent memory pool

		p_ctrl_block m_next; // next block in the linked list (if the blocked cached in the bins array)
		p_ctrl_block m_prev; // previoius block in the linked list (if the blocked cached in the bins array)

		DELETE_CONSTRUCTOR_AND_DESTRUCTOR(m_ctrl_block);

		INLINE p_pool_local pool()
		{
			return m_pool;
		}

		INLINE void pool(p_pool_local pool)
		{
			m_pool = pool;
		}

		INLINE size_t head()
		{
			return m_head;
		}

		INLINE void head(size_t head)
		{
			m_head = head;
		}

		INLINE size_t size()
		{
			return m_data & (~CBit) & (~PBit);
		}

		INLINE void size(size_t size)
		{
			m_data = (m_data & (CBit | PBit)) | (size & (~CBit) & (~PBit));
		}

		INLINE size_t pbit() // flag, whether the previous control memory block is in use
		{
			return m_data & PBit;
		}

		INLINE size_t cbit() // flag, whether the current control memory block is in use
		{
			return m_data & CBit;
		}

		INLINE void turn(size_t bit)
		{
			m_data |= bit;
		}

		INLINE void drop(size_t bit)
		{
			m_data &= ~bit;
		}

		INLINE p_ctrl_block next_blck()
		{
			return add_mem<p_ctrl_block>(this, size());
		}

		INLINE p_ctrl_block prev_blck()
		{
			return sub_mem<p_ctrl_block>(this, head());
		}

		INLINE void* user_blck()
		{
			return reinterpret_cast<char*>(this) + sizeof(m_ctrl_block);
		}
	};

}; // namespace Small


////////////////////////////////////////////////////////////////////////////////

namespace Small
{
	// this routine is used to convert user memory to allocator internal control 
	// memory block
	INLINE static p_ctrl_block mem_to_blk(void* mem)
	{
		return sub_mem<p_ctrl_block>(mem, sizeof(m_ctrl_block));
	}

}; //namespace Small


////////////////////////////////////////////////////////////////////////////////

namespace Small
{
	// This data structure describes a unique memory pool.
	struct m_pool_local
	{
		enum
		{
			Count = 32,			
			MaxUserReqSize = 256
		};

		LOCK         m_lock; // mutex to lock the whole pool
		p_ctrl_block m_foot; // free control memory block which is used to allocate new memory blocks

		uint32_t     m_bits; // binary map used to indicate what bins are in the use
		m_ctrl_block m_bins[Count]; // array of linked lists used to cache already freed memory blocks

		p_pool_local m_next; // link to next memory pool in this allocator
		DELETE_CONSTRUCTOR_AND_DESTRUCTOR(m_pool_local);

		INLINE void init(size_t foot_size)
		{
			m_bits = 0;
			m_lock.mutex::mutex();

			m_foot = add_mem<p_ctrl_block>(this, sizeof(m_pool_local));
			m_foot->size(foot_size - sizeof(m_pool_local));
			m_foot->turn(PBit);

			// init list bins
			for (size_t i = 0; i < Count; i++)
				m_bins[i].m_next = m_bins[i].m_prev = bins_blck(i);
		}

		INLINE void fini()
		{
			for (size_t i = 0; i < Count; i++)
			{
				assert(m_bins[i].m_next == m_bins[i].m_prev);
				assert(m_bins[i].m_next == bins_blck(i));
			}
			assert(m_foot == add_mem<p_ctrl_block>(this, sizeof(m_pool_local)));
		}

		// this routine tries to allocate memory block; 
		// first it looks to the binary maps for suitable memory block,
		// and later if there is enough memory is allocates a new block
		// from the foots
		void* malloc(size_t bytesreq)
		{
			if (!m_lock.try_lock())
				return VOID_0;
			
			if (bytesreq >= MaxUserReqSize)
				return VOID_1;

			void* mem = NULL;

			size_t size = (bytesreq + sizeof(m_ctrl_block) + 0x7) & ~0x7; //adjusting the size to double word boundary
			size_t indx = size >> 3;

			size_t bit = m_bits >> indx;
			if (bit & 0x0000001)
			{
				mem = bins_malloc(size);
			}
			else if (size < m_foot->size())
			{
				mem = foot_malloc(size);
			}			

			m_lock.unlock();
			return mem != NULL ? mem : VOID_1;
		}

		// this routine releases allocated memory block; it tries to coalesce
		// it with the previous or the next block, and then caches the result
		// in the binary map
		void free(void* p)
		{
			SCOPE_LOCK(m_lock);

			p_ctrl_block curr_b = mem_to_blk(p);
			size_t       curr_s = curr_b->size();

			assert(curr_b->pool() == this);

			if (!curr_b->cbit())
				return;

			p_ctrl_block next_b = curr_b->next_blck();
			size_t       next_s = next_b->size();

			if (!curr_b->pbit()) //coalesce with previous block
			{
				size_t       prev_s = curr_b->head();
				p_ctrl_block prev_b = curr_b->prev_blck();

				pull_binblk(prev_b);

				curr_b = prev_b;
				curr_s += prev_s;
			}

			if (!next_b->cbit()) //coalesce with next block
			{
				curr_s += next_s;

				if (next_b == m_foot)
				{
					m_foot = curr_b;
					m_foot->size(curr_s);
					m_foot->turn(PBit);
					m_foot->drop(CBit);

					return;
				}
				else
				{
					pull_binblk(next_b);
				}
			}

			curr_b->drop(CBit);
			curr_b->size(curr_s);
			curr_b->next_blck()->drop(PBit);
			curr_b->next_blck()->head(curr_s);

			push_binblk(curr_b);
		}

		INLINE p_ctrl_block bins_blck(size_t indx)
		{
			assert(indx < Count);
			return &m_bins[indx];
		}

		// the index in the array of linked lists; 
		// map size value to the array of 32 bins;
		INLINE size_t bins_indx(size_t size)
		{
			return size >> 3;
		}

		// takes the first block in the specified linked list
		INLINE void* bins_malloc(size_t size)
		{
			size_t indx = bins_indx(size);
			assert((bins_blck(indx)->m_next != bins_blck(indx)) && (bins_blck(indx)->m_prev != bins_blck(indx)));

			p_ctrl_block blck = bins_blck(indx)->m_next;

			blck->pool(this);
			blck->turn(CBit);
			blck->turn(PBit);

			pull_binblk(blck);
			return blck->user_blck();
		}

		// allocates a new memory block on the foot
		INLINE void* foot_malloc(size_t size)
		{
			size_t rest = m_foot->size() - size;

			p_ctrl_block blck = m_foot;
			blck->pool(this);
			blck->size(size);
			blck->turn(CBit);

			m_foot = blck->next_blck();
			m_foot->size(rest);
			m_foot->head(size);
			m_foot->turn(PBit);
			m_foot->drop(CBit);

			return blck->user_blck();
		}

		// add memory block to the specified linked list
		INLINE void push_binblk(p_ctrl_block blck)
		{
			size_t size = blck->size();
			size_t indx = bins_indx(size);

			assert(indx < Count);

			p_ctrl_block prev = bins_blck(indx);
			p_ctrl_block next = prev;

			prev->m_next = blck;
			next->m_prev = blck;
			blck->m_next = next;
			blck->m_prev = prev;

			m_bits |= ((size_t)1 << indx);
		}

		// remove memory block from the specified linked list
		INLINE void pull_binblk(p_ctrl_block blck)
		{
			p_ctrl_block lblk = static_cast<p_ctrl_block>(blck);

			size_t size = lblk->size();
			size_t indx = bins_indx(size);

			assert(indx < Count);

			p_ctrl_block next = blck->m_next;
			p_ctrl_block prev = blck->m_prev;

			if (next == prev) // drop bits
				m_bits &= ~((size_t)1 << indx);

			next->m_prev = prev;
			prev->m_next = next;
		}
	};

}; // namespace Small


//===================================================================================
//
//

namespace Small
{

BlockAllocator::BlockAllocator(size_t thread_local_capacity)
	: m_ThreadCount(0)
{
	for (size_t i = 0; i < MaxThreadCount; i++)
	{
		m_ThreadPool[i] = pool_construct(thread_local_capacity);
	}
	for (size_t i = 0; i < MaxThreadCount; i++)
	{
		assert(m_ThreadPool[i]);
	}
	for (size_t i = 0; i < MaxThreadCount - 1; i++)
	{
		m_ThreadPool[i]->m_next = m_ThreadPool[i + 1];
	}
	m_ThreadPool[MaxThreadCount - 1]->m_next = m_ThreadPool[0];
}


/////////////////////////////////////////////////////////////////////////////////////

BlockAllocator::~BlockAllocator()
{
	for (size_t i = 0; i < MaxThreadCount; i++)
	{
		pool_destruct(m_ThreadPool[i]);
	}
}


/////////////////////////////////////////////////////////////////////////////////////

void* BlockAllocator::malloc(size_t size)
{
	void*  umem = NULL;

	if (m_ThreadIndex == (uint16_t)(-1))
		m_ThreadIndex = m_ThreadCount++;

	assert(m_ThreadIndex < MaxThreadCount);
	p_pool_local pool = m_ThreadPool[m_ThreadIndex];

	size_t indx = 0;
	size_t bits = 0;
	size_t flag = 0;
	size_t mask = ((size_t)1 << MaxThreadCount) - 1;

	// run through the circular list of the pools trying to lock one;
	// the pool return 1u if it is cannot allocate the block; then try
	// the next pool until we look over all the pools
	do
	{
		umem = pool->malloc(size);
		pool = pool->m_next;

		flag = reinterpret_cast<size_t>(umem);
		bits |= (flag << indx);

		flag &= ~0x1;
		indx += 1;
	} 
	while ((bits ^ mask) && !flag);

	return CAST(flag);
}


/////////////////////////////////////////////////////////////////////////////////////

void BlockAllocator::free(void* umem)
{
	if (!umem)
		return;

	p_ctrl_block blck = mem_to_blk(umem);
	p_pool_local pool = blck->pool();

	if (pool)
		pool->free(umem);
}

/////////////////////////////////////////////////////////////////////////////////////

p_pool_local BlockAllocator::pool_construct(size_t capacity)
{
	p_pool_local pool = NULL;

	SYSTEM_INFO info;
	::GetSystemInfo(&info);

	size_t gran = info.dwAllocationGranularity;
	size_t size = (capacity + (gran << 1) - 1) & ~(gran - 1); // align capacity to granularity size

	void* memory = ::VirtualAlloc(0, size, MEM_RESERVE | MEM_COMMIT, PAGE_READWRITE);
	if (!memory)
		return NULL;

	::ZeroMemory(memory, size);

	pool = static_cast<p_pool_local>(memory);
	pool->init(size);

	return pool;
}

////////////////////////////////////////////////////////////////////////////////

void BlockAllocator::pool_destruct(p_pool_local pool)
{
	if (pool)
	{
		pool->fini();
		::VirtualFree(pool, 0, MEM_RELEASE);
	}
}


////////////////////////////////////////////////////////////////////////////////

uint16_t BlockAllocator::m_ThreadIndex = (uint16_t)-1;

}; //namespace Small

