#include "pch.h"
#include "LinearAllocator.h"
#include <stdlib.h>
#include <malloc.h>

LinearAllocator::LinearAllocator(uint64_t TotalMemorySizeInBytes)
	: TotalMemorySizeInBytes(TotalMemorySizeInBytes)
{
	pHostMemory = malloc(TotalMemorySizeInBytes);
	Offset = 0;
}

LinearAllocator::~LinearAllocator()
{
	free(pHostMemory);
}


void* LinearAllocator::Allocate(uint64_t SizeInBytes)
{
	const uint64_t CurrentAddress = (uint64_t)pHostMemory + Offset;

	if (Offset + SizeInBytes > TotalMemorySizeInBytes)
		return nullptr;

	Offset += SizeInBytes;
	return (void*)CurrentAddress;
}

void LinearAllocator::Reset()
{
	Offset = 0;
}