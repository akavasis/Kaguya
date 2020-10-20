#pragma once
#include <stdint.h>

class LinearAllocator
{
public:
	LinearAllocator(uint64_t TotalMemorySizeInBytes);
	~LinearAllocator();

	void* Allocate(uint64_t SizeInBytes);

	void Reset();
private:
	const uint64_t TotalMemorySizeInBytes;

	void* pHostMemory;
	uint64_t Offset;
};