#include "pch.h"
#include "Texture.h"

Texture::Texture(Microsoft::WRL::ComPtr<ID3D12Resource> ExistingID3D12Resource)
	: Resource(ExistingID3D12Resource)
{
	D3D12_RESOURCE_DESC desc = m_pResource->GetDesc();
	switch (desc.Dimension)
	{
	case D3D12_RESOURCE_DIMENSION_BUFFER: m_Type = Resource::Type::Buffer; break;
	case D3D12_RESOURCE_DIMENSION_TEXTURE1D: m_Type = Resource::Type::Texture1D; break;
	case D3D12_RESOURCE_DIMENSION_TEXTURE2D: m_Type = Resource::Type::Texture2D; break;
	case D3D12_RESOURCE_DIMENSION_TEXTURE3D: m_Type = Resource::Type::Texture3D; break;
	default: m_Type = Resource::Type::Unknown; break;
	}
	m_Format = desc.Format;
	m_Width = desc.Width;
	m_Height = desc.Height;
	m_DepthOrArraySize = desc.DepthOrArraySize;
	m_MipLevels = desc.MipLevels;
}

Texture::Texture(const Device* pDevice, const Properties& Properties)
	: Resource(pDevice, Resource::Properties::Texture(Properties.Type,
		Properties.Format,
		Properties.Width,
		Properties.Height,
		Properties.DepthOrArraySize,
		Properties.MipLevels,
		Properties.Flags,
		Properties.pOptimizedClearValue), Properties.InitialState),
	m_Type(Properties.Type),
	m_Format(Properties.Format),
	m_Width(Properties.Width),
	m_Height(Properties.Height),
	m_DepthOrArraySize(Properties.DepthOrArraySize),
	m_MipLevels(Properties.MipLevels),
	m_IsCubemap(Properties.IsCubemap)
{
}

Texture::Texture(const Device* pDevice, const Properties& Properties, const Heap* pHeap, UINT64 HeapOffset)
	: Resource(pDevice, Resource::Properties::Texture(Properties.Type,
		Properties.Format,
		Properties.Width,
		Properties.Height,
		Properties.DepthOrArraySize,
		Properties.MipLevels,
		Properties.Flags,
		Properties.pOptimizedClearValue), Properties.InitialState, pHeap, HeapOffset),
	m_Type(Properties.Type),
	m_Format(Properties.Format),
	m_Width(Properties.Width),
	m_Height(Properties.Height),
	m_DepthOrArraySize(Properties.DepthOrArraySize),
	m_MipLevels(Properties.MipLevels),
	m_IsCubemap(Properties.IsCubemap)
{
}

Texture::~Texture()
{
}

bool Texture::IsArray() const
{
	return (m_Type == Resource::Type::Texture1D || m_Type == Resource::Type::Texture2D) && m_DepthOrArraySize > 1;
}

bool Texture::IsCubemap() const
{
	return m_IsCubemap;
}