#pragma once
#include <d3d12.h>
#include "Proxy.h"
#include "../D3D12/Heap.h"

class HeapProxy : public Proxy
{
public:
	friend class Heap;
	HeapProxy();

	UINT64 SizeInBytes;	//< Default value: 0, must be set
	Heap::Type Type;	//< Default value: Default, optional set
	Heap::Flags Flags;	//< Default value: None, optional set
protected:
	void Link() override;
private:
	D3D12_HEAP_DESC BuildD3DDesc();
};