#pragma once
#include "Proxy.h"
#include "../Heap.h"

class HeapProxy : public Proxy
{
	friend class Heap;
public:
	HeapProxy();
	~HeapProxy() override = default;

	void SetSizeInBytes(UINT64 SizeInBytes);
	void SetType(Heap::Type Type);
	void SetFlags(Heap::Flags Flags);
protected:
	void Link() override;
	D3D12_HEAP_DESC GetD3DHeapDesc();
private:
	UINT64 m_SizeInBytes;	//< Default value: 0, must be set
	Heap::Type m_Type;		//< Default value: Default, optional set
	Heap::Flags m_Flags;	//< Default value: None, optional set
};