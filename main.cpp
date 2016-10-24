//===================================================================================
//
//
#include "small_block_allocator.hpp"
#include "large_block_allocator.hpp"


/////////////////////////////////////////////////////////////////////////////////////
void main()
{
	LargeBlockAllocator allocator;

	void* p0  = allocator.malloc((1 << 7) + 4 * 0);
	void* p00 = allocator.malloc(1);
	void* p1  = allocator.malloc((1 << 7) + 4 * 1);
	void* p10 = allocator.malloc(1);
	void* p2  = allocator.malloc((1 << 7) + 4 * 2);
	void* p20 = allocator.malloc(1);
	void* p3  = allocator.malloc((1 << 7) + 4 * 3);
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


	int a = 1;
	return;
}