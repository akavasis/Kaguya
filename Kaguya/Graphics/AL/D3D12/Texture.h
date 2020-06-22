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

		D3D12_CLEAR_VALUE* pOptimizedClearValue;
		ResourceStates InitialState;
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
private:
	Resource::Type m_Type;
	DXGI_FORMAT m_Format;
	UINT64 m_Width;
	UINT m_Height;
	UINT16 m_DepthOrArraySize;
	UINT16 m_MipLevels;
};