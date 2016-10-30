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
//
// Implementation is not multiplatform, but could be done multiplatform with some effort;
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