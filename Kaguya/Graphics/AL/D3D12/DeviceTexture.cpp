#include "pch.h"
#include "DeviceTexture.h"
#include "Device.h"
#include "../Proxy/DeviceTextureProxy.h"

DeviceTexture::DeviceTexture(Microsoft::WRL::ComPtr<ID3D12Resource> ExistingID3D12Resource)
	: DeviceResource(ExistingID3D12Resource),
	m_Format(ExistingID3D12Resource->GetDesc().Format),
	m_Width(ExistingID3D12Resource->GetDesc().Width),
	m_Height(ExistingID3D12Resource->GetDesc().Height),
	m_DepthOrArraySize(ExistingID3D12Resource->GetDesc().DepthOrArraySize),
	m_MipLevels(ExistingID3D12Resource->GetDesc().MipLevels)
{
}

DeviceTexture::DeviceTexture(const Device* pDevice, DeviceTextureProxy& Proxy)
	: DeviceResource(pDevice, Proxy),
	m_Format(Proxy.m_Format),
	m_Width(Proxy.m_Width),
	m_Height(Proxy.m_Height),
	m_DepthOrArraySize(Proxy.m_DepthOrArraySize),
	m_MipLevels(Proxy.m_MipLevels)
{
	m_MipLevels = m_pResource->GetDesc().MipLevels;
}

DeviceTexture::DeviceTexture(const Device* pDevice, const Heap* pHeap, UINT64 HeapOffset, DeviceTextureProxy& Proxy)
	: DeviceResource(pDevice, pHeap, HeapOffset, Proxy),
	m_Format(Proxy.m_Format),
	m_Width(Proxy.m_Width),
	m_Height(Proxy.m_Height),
	m_DepthOrArraySize(Proxy.m_DepthOrArraySize),
	m_MipLevels(Proxy.m_MipLevels)
{
	m_MipLevels = m_pResource->GetDesc().MipLevels;
}

DeviceTexture::~DeviceTexture()
{
}

bool DeviceTexture::IsArray() const
{
	return (m_Type == DeviceResource::Type::Texture1D || m_Type == DeviceResource::Type::Texture2D || m_Type == DeviceResource::Type::TextureCube) && m_DepthOrArraySize > 1;
}