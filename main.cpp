//===================================================================================
// 
// Just finished what i haven't caught up to do in the previous version:
// united SmallBlockAllocator and LargeBlockAllocator to one
// BlockAllocator class to manage effectively small and large memory blocks in
// one allocator class.
//
// Brief description:
//
// BlockAllocator has two internal structures defined:
// m_ctrl_block - which acts as a "service" header for user allocated block of memory; and
// m_pool_local - structure which acts as thread local memory pool and manages (allocates and caches)
// requested memory blocks; m_ctrl_pool uses two binary map inside to cache already freed memory block
// for future use: 1) array of linked lists to cache small size memory blocks (<256 bytes) and 
// 2) array of binary trees to cache large size memory block (>256 bytes); 
// Also when freeing an already allocated memory block, first trying to coalesce it with the
// neighboring memory block if they are free and only cache it to treemap or listmap (depending on size of the block).
//
//
//===================================================================================
//
//
#include <thread>
#include <cstdlib>

#include "block_allocator.hpp"


/////////////////////////////////////////////////////////////////////////////////////
void main()
{
	enum
	{
		ThreadCount = 1
	};

	BlockAllocator allocator;

	std::thread thread[ThreadCount];
	for (size_t i = 0; i < ThreadCount; i++)
	{
		thread[i] = std::thread([&]()
		{
			std::this_thread::sleep_for(std::chrono::milliseconds(ThreadCount * 10 - i * 10));

			for (size_t i = 0; i < 10000; i++)
			{
				void* p0 = allocator.malloc((1 << 7) + 4 * 0);
				void* p00 = allocator.malloc(1);
				void* p1 = allocator.malloc((1 << 7) + 4 * 1);
				void* p10 = allocator.malloc(1);
				void* p2 = allocator.malloc((1 << 7) + 4 * 2);
				void* p20 = allocator.malloc(1);
				void* p3 = allocator.malloc((1 << 7) + 4 * 3);
				void* p30 = allocator.malloc(1);

				allocator.free(p0);
				allocator.free(p1);
				allocator.free(p2);
				allocator.free(p3);

				p0 = allocator.malloc(17);
				p1 = allocator.malloc(18);
				p2 = allocator.malloc(19);
				p3 = allocator.malloc(20);

				allocator.free(p0);
				allocator.free(p1);
				allocator.free(p2);
				allocator.free(p3);

				allocator.free(p00);
				allocator.free(p10);
				allocator.free(p20);
				allocator.free(p30);
			}
		});
	}
	for (size_t i = 0; i < ThreadCount; i++)
	{
		thread[i].join();
	}
}