#include "pch.h"
#include "Texture.h"

Texture::Texture(Microsoft::WRL::ComPtr<ID3D12Resource> ExistingID3D12Resource)
	: Resource(ExistingID3D12Resource)
{
	D3D12_RESOURCE_DESC desc = m_pResource->GetDesc();
	switch (desc.Dimension)
	{
	case D3D12_RESOURCE_DIMENSION_TEXTURE1D: Type = Resource::Type::Texture1D; break;
	case D3D12_RESOURCE_DIMENSION_TEXTURE2D: Type = Resource::Type::Texture2D; break;
	case D3D12_RESOURCE_DIMENSION_TEXTURE3D: Type = Resource::Type::Texture3D; break;
	default: Type = Resource::Type::Unknown; break;
	}
	Format = desc.Format;
	Width = desc.Width;
	Height = desc.Height;
	DepthOrArraySize = desc.DepthOrArraySize;
	MipLevels = desc.MipLevels;
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
	Type(Properties.Type),
	Format(Properties.Format),
	Width(Properties.Width),
	Height(Properties.Height),
	DepthOrArraySize(Properties.DepthOrArraySize),
	MipLevels(Properties.MipLevels),
	IsCubemap(Properties.IsCubemap)
{
	D3D12_RESOURCE_DESC desc = m_pResource->GetDesc();
	MipLevels = desc.MipLevels;
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
	Type(Properties.Type),
	Format(Properties.Format),
	Width(Properties.Width),
	Height(Properties.Height),
	DepthOrArraySize(Properties.DepthOrArraySize),
	MipLevels(Properties.MipLevels),
	IsCubemap(Properties.IsCubemap)
{
	D3D12_RESOURCE_DESC desc = m_pResource->GetDesc();
	MipLevels = desc.MipLevels;
}

bool Texture::IsArray() const
{
	return (Type == Resource::Type::Texture1D || Type == Resource::Type::Texture2D) && DepthOrArraySize > 1;
}