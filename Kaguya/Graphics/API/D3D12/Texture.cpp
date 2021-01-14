#include "pch.h"
#include "Texture.h"
#include "Device.h"
#include "../Proxy/TextureProxy.h"

Texture::Texture(Microsoft::WRL::ComPtr<ID3D12Resource> ExistingID3D12Resource)
	: Resource(ExistingID3D12Resource)
	, Format(ExistingID3D12Resource->GetDesc().Format)
	, Width(ExistingID3D12Resource->GetDesc().Width)
	, Height(ExistingID3D12Resource->GetDesc().Height)
	, DepthOrArraySize(ExistingID3D12Resource->GetDesc().DepthOrArraySize)
	, MipLevels(ExistingID3D12Resource->GetDesc().MipLevels)
{

}

Texture::Texture(ID3D12Device* pDevice, TextureProxy& Proxy)
	: Resource(pDevice, Proxy)
	, Format(Proxy.m_Format)
	, Width(Proxy.m_Width)
	, Height(Proxy.m_Height)
	, DepthOrArraySize(Proxy.m_DepthOrArraySize)
	, MipLevels(Proxy.m_MipLevels)
{
	MipLevels = m_pResource->GetDesc().MipLevels;
}

Texture::Texture(ID3D12Device* pDevice, ID3D12Heap* pHeap, UINT64 HeapOffset, TextureProxy& Proxy)
	: Resource(pDevice, pHeap, HeapOffset, Proxy)
	, Format(Proxy.m_Format)
	, Width(Proxy.m_Width)
	, Height(Proxy.m_Height)
	, DepthOrArraySize(Proxy.m_DepthOrArraySize)
	, MipLevels(Proxy.m_MipLevels)
{
	MipLevels = m_pResource->GetDesc().MipLevels;
}

bool Texture::IsArray() const
{
	return (m_Type == Resource::Type::Texture1D || m_Type == Resource::Type::Texture2D || m_Type == Resource::Type::TextureCube) && DepthOrArraySize > 1;
}