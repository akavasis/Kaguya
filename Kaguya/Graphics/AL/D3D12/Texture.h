#pragma once
#include "Resource.h"

class Texture : public Resource
{
public:
	struct Properties
	{
		Resource::Type Type;
		DXGI_FORMAT Format;
		UINT64 Width;
		UINT Height;
		UINT16 DepthOrArraySize;
		UINT16 MipLevels;
		D3D12_RESOURCE_FLAGS Flags;
		bool IsCubemap;

		D3D12_CLEAR_VALUE* pOptimizedClearValue;
		D3D12_RESOURCE_STATES InitialState;
	};

	Texture() = default;
	Texture(Microsoft::WRL::ComPtr<ID3D12Resource> ExistingID3D12Resource);
	Texture(const Device* pDevice, const Properties& Properties);
	Texture(const Device* pDevice, const Properties& Properties, const Heap* pHeap, UINT64 HeapOffset);
	~Texture() override = default;

	Texture(Texture&&) = default;
	Texture& operator=(Texture&&) = default;

	Texture(const Texture&) = delete;
	Texture& operator=(const Texture&) = delete;

	bool IsArray() const;

	Resource::Type Type = Resource::Type::Unknown;
	DXGI_FORMAT Format = DXGI_FORMAT_UNKNOWN;
	UINT64 Width = 0;
	UINT Height = 0;
	UINT16 DepthOrArraySize = 0;
	UINT16 MipLevels = 0;
	bool IsCubemap = false;
};