#pragma once
#include "Resource.h"

class TextureProxy;

struct Texture : public Resource
{
	Texture() = default;
	Texture(Microsoft::WRL::ComPtr<ID3D12Resource> ExistingID3D12Resource);
	Texture(ID3D12Device* pDevice, TextureProxy& Proxy);
	Texture(ID3D12Device* pDevice, ID3D12Heap* pHeap, UINT64 HeapOffset, TextureProxy& Proxy);

	bool IsArray() const;

	DXGI_FORMAT Format;
	UINT64		Width;
	UINT		Height;
	UINT16		DepthOrArraySize;
	UINT16		MipLevels;
};