//===================================================================================
//
//

#include "small_block_allocator.hpp"
#include "large_block_allocator.hpp"

/////////////////////////////////////////////////////////////////////////////////////
void main()
{
	SmallBlockAllocator allocator;

	void* p0 = allocator.malloc(170000);
	void* p1 = allocator.malloc(17);

	allocator.free(p0);

	void* p2 = allocator.malloc(17);

	allocator.free(p2);
	allocator.free(p2);
	allocator.free(p1);

	int a = 1;
	return;
}