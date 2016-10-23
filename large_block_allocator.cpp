//===================================================================================
//
// externals:

#include "large_block_allocator.hpp"


//===================================================================================
//
// forward decls:

namespace Large
{
	struct m_ctrl_block;
	struct m_pool_local;

	using  p_ctrl_block = m_ctrl_block*;
	using  p_pool_local = m_pool_local*;

}; // namespace Large


//===================================================================================
//
// publics:

namespace Large
{
	struct m_ctrl_block
	{
		size_t       m_head; //
		size_t       m_data; //

		p_pool_local m_pool; // parent memory pool

		size_t       m_indx;

		p_ctrl_block m_next;
		p_ctrl_block m_prev;

		p_ctrl_block m_limb[2];
		p_ctrl_block m_parent;

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

		INLINE p_pool_local pool()
		{
			return m_pool;
		}

		INLINE void pool(p_pool_local pool)
		{
			m_pool = pool;
		}

		INLINE size_t indx()
		{
			return m_indx;
		}

		INLINE void indx(size_t indx)
		{
			m_indx = indx;
		}

		INLINE size_t pbit()
		{
			return m_data & PBit;
		}

		INLINE size_t cbit()
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

		INLINE p_ctrl_block left_most_limb()
		{
			return m_limb[0] != 0 ? m_limb[0] : m_limb[1];
		}

		INLINE void* user_blck()
		{
			return reinterpret_cast<char*>(this) + sizeof(m_ctrl_block);
		}

		DELETE_CONSTRUCTOR_AND_DESTRUCTOR(m_ctrl_block);
	};

}; // namespace Large


////////////////////////////////////////////////////////////////////////////////

namespace Large
{
	INLINE static p_ctrl_block mem_to_blk(void* mem)
	{
		return sub_mem<p_ctrl_block>(mem, sizeof(m_ctrl_block));
	}
};


////////////////////////////////////////////////////////////////////////////////

namespace Large
{

	struct m_pool_local
	{
		enum
		{
			Count = 0x20,
			PowerOfTwo = 8,
			MinBlckSize = (sizeof(m_ctrl_block) + 0x7) & ~0x7,
			MinRequestSize = MinBlckSize - sizeof(m_ctrl_block),
			MaxRequestSize = (-MinBlckSize) << 2,
			MaxBinBlockSize = (1 << PowerOfTwo) - 1,
			MaxBinBlockRequest = MaxBinBlockSize - sizeof(m_ctrl_block) - 0x7
		};

		LOCK         m_lock;
		p_ctrl_block m_foot;

		uint32_t     m_bits;
		p_ctrl_block m_bins[Count];

		p_pool_local m_next;
		DELETE_CONSTRUCTOR_AND_DESTRUCTOR(m_pool_local);

		INLINE void init(size_t foot_size)
		{
			m_bits = 0;
			m_lock.mutex::mutex();

			m_foot = add_mem<p_ctrl_block>(this, sizeof(m_pool_local));
			m_foot->size(foot_size - sizeof(m_pool_local));
			m_foot->turn(PBit);
		}

		INLINE void fini()
		{
			//for (size_t i = 0; i < Count; i++)
			//{
			//	assert(m_bins[i].m_next == m_bins[i].m_prev);
			//	assert(m_bins[i].m_next == bins(i));
			//}
			assert(m_foot == add_mem<p_ctrl_block>(this, sizeof(m_pool_local)));
		}

		void* malloc(size_t bytesreq)
		{
			if (!m_lock.try_lock())
				return NULL;

			void* mem = NULL;

			size_t size = (bytesreq + sizeof(m_ctrl_block) + 0x7) & ~0x7;
			size_t indx = bins_indx(size);

			if ((m_bits >> indx) & 1u)
			{
				mem = bins_malloc(size);
			}
			else if (size < m_foot->size())
			{
				mem = foot_malloc(size);
			}

			m_lock.unlock();
			return (mem != NULL) ? mem : reinterpret_cast<void*>(1u);
		}

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

			curr_b->size(curr_s);
			curr_b->drop(CBit);
			curr_b->next_blck()->drop(PBit);

			push_binblk(curr_b);
		}

		INLINE void* bins_malloc(size_t size)
		{
			p_ctrl_block topt = bins_blck(bins_indx(size));
			p_ctrl_block blck = topt;

			size_t rsize = topt->size() - size;

			while (topt = topt->left_most_limb())
			{
				size_t srem = topt->size() - size;
				if (srem < rsize)
				{
					rsize = srem;
					blck = topt;
				}
			}

			pull_binblk(blck);

			blck->pool(this);
			blck->turn(CBit);
			blck->turn(PBit);

			return blck->user_blck();
		}

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

		INLINE p_ctrl_block bins_blck(size_t indx)
		{
			assert(indx < Count);
			return m_bins[indx];
		}

		INLINE size_t bins_indx(size_t size)
		{
			DWORD indx;
			BSR(&indx, size);

			return indx;
		}

		INLINE void push_binblk(p_ctrl_block blck)
		{
			size_t size = blck->size();
			size_t indx = bins_indx(size);

			blck->indx(indx);

			blck->m_limb[0] = NULL;
			blck->m_limb[1] = NULL;

			size_t mask = (size_t)1u << indx;
			if ((m_bits & mask) == 0)
			{
				m_bits |= mask;
				m_bins[indx] = blck;

				blck->m_parent = m_bins[indx];

				blck->m_next = blck;
				blck->m_prev = blck;

				return;
			}

			p_ctrl_block topt = bins_blck(indx);

			for (;;)
			{
				size_t H = (size_t)1u << 31u;
				size_t R = size << (31u - indx);

				if (topt->size() != size)
				{
					p_ctrl_block* c = &topt->m_limb[R & H];
					R <<= 1;

					if (*c)
					{
						topt = *c;
					}
					else
					{
						*c = blck;

						blck->m_parent = topt;

						blck->m_next = blck;
						blck->m_prev = blck;

						break;
					}
				}
				else
				{
					p_ctrl_block next = topt->m_next;

					topt->m_next = blck;
					next->m_prev = blck;

					blck->m_next = next;
					blck->m_prev = topt;

					blck->m_parent = NULL;
					break;
				}
			}
		}

		INLINE void pull_binblk(p_ctrl_block blck)
		{
			p_ctrl_block prnt = blck->m_parent;

			p_ctrl_block r;

			if (blck->m_prev != blck)
			{
				p_ctrl_block next = blck->m_next;
				p_ctrl_block prev = blck->m_prev;

				next->m_prev = prev;
				prev->m_next = next;
			}
			else
			{
				p_ctrl_block* rp;
				if (((r = *(rp = &(blck->m_limb[0]))) != 0) || ((r = *(rp = &(blck->m_limb[1]))) != 0))
				{
					p_ctrl_block* cp;
					while ((*(cp = &(blck->m_limb[0])) != 0) || (*(cp = &(blck->m_limb[1])) != 0))
					{
						r = *(rp = cp);
					}

					*rp = 0;
				}
			}

			if (prnt)
			{
				size_t indx = blck->indx();
				p_ctrl_block* h = &m_bins[indx];

				if (blck == *h)
				{
					if ((*h == r) == 0)
					{
						m_bits &= ~((size_t)1u << indx);
					}
				}
				else
				{
					if (prnt->m_limb[0] == blck)
					{
						prnt->m_limb[0] = r;
					}
					else
					{
						prnt->m_limb[1] = r;
					}
				}

				if (r)
				{
					p_ctrl_block c0, c1;
					r->m_parent = prnt;

					if ((c0 = blck->m_limb[0]) != 0)
					{
						r->m_limb[0] = c0;
						c0->m_parent = r;
					}
					if ((c1 = blck->m_limb[1]) != 0)
					{
						r->m_limb[1] = c1;
						c1->m_parent = r;
					}
				}
			}
		}
	};

}; //namespace Large


//===================================================================================
//
//

namespace Large
{

BlockAllocator::BlockAllocator(size_t thread_local_capacity)
	: m_ThreadCount(0)
{
	for (size_t i = 0; i < MaxThreadCount; i++)
	{
		m_ThreadPool[i] = pool_construct(thread_local_capacity);
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

	do
	{
		umem = pool->malloc(size);
		pool = pool->m_next;

		flag = reinterpret_cast<size_t>(umem);
		bits |= (flag << indx);

		flag &= ~0x1;
		indx += 1;
	} while ((bits ^ mask) && !flag);

	return reinterpret_cast<void*>(flag);
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

uint16_t BlockAllocator::m_ThreadIndex = (uint16_t)-1;

};

