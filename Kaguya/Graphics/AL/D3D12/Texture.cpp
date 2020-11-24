#include "pch.h"
#include "Texture.h"
#include "Device.h"
#include "../Proxy/TextureProxy.h"

Texture::Texture(Microsoft::WRL::ComPtr<ID3D12Resource> ExistingID3D12Resource)
	: Resource(ExistingID3D12Resource),
	m_Format(ExistingID3D12Resource->GetDesc().Format),
	m_Width(ExistingID3D12Resource->GetDesc().Width),
	m_Height(ExistingID3D12Resource->GetDesc().Height),
	m_DepthOrArraySize(ExistingID3D12Resource->GetDesc().DepthOrArraySize),
	m_MipLevels(ExistingID3D12Resource->GetDesc().MipLevels)
{

}

Texture::Texture(const Device* pDevice, TextureProxy& Proxy)
	: Resource(pDevice, Proxy),
	m_Format(Proxy.m_Format),
	m_Width(Proxy.m_Width),
	m_Height(Proxy.m_Height),
	m_DepthOrArraySize(Proxy.m_DepthOrArraySize),
	m_MipLevels(Proxy.m_MipLevels)
{
	m_MipLevels = m_pResource->GetDesc().MipLevels;
}

Texture::Texture(const Device* pDevice, const Heap* pHeap, UINT64 HeapOffset, TextureProxy& Proxy)
	: Resource(pDevice, pHeap, HeapOffset, Proxy),
	m_Format(Proxy.m_Format),
	m_Width(Proxy.m_Width),
	m_Height(Proxy.m_Height),
	m_DepthOrArraySize(Proxy.m_DepthOrArraySize),
	m_MipLevels(Proxy.m_MipLevels)
{
	m_MipLevels = m_pResource->GetDesc().MipLevels;
}

Texture::~Texture()
{
}

bool Texture::IsArray() const
{
	return (m_Type == Resource::Type::Texture1D || m_Type == Resource::Type::Texture2D || m_Type == Resource::Type::TextureCube) && m_DepthOrArraySize > 1;
}