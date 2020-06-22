#include "pch.h"
#include "Texture.h"

Texture::Texture(Microsoft::WRL::ComPtr<ID3D12Resource> ExistingID3D12Resource)
	: Resource(ExistingID3D12Resource)
{
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
	m_MipLevels(Properties.MipLevels)
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
	m_MipLevels(Properties.MipLevels)
{
}

Texture::~Texture()
{
}

bool Texture::IsArray() const
{
	return (m_Type == Resource::Type::Texture1D || m_Type == Resource::Type::Texture2D) && m_DepthOrArraySize > 1;
}