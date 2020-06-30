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
	~Texture() override;

	Texture(Texture&&) = default;
	Texture& operator=(Texture&&) = default;

	Texture(const Texture&) = delete;
	Texture& operator=(const Texture&) = delete;

	bool IsArray() const;
	bool IsCubemap() const;
private:
	Resource::Type m_Type = Resource::Type::Unknown;
	DXGI_FORMAT m_Format = DXGI_FORMAT_UNKNOWN;
	UINT64 m_Width = 0;
	UINT m_Height = 0;
	UINT16 m_DepthOrArraySize = 0;
	UINT16 m_MipLevels = 0;
	bool m_IsCubemap = false;
};