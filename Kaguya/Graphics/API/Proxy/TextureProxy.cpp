#include "pch.h"
#include "TextureProxy.h"
#include "../D3D12/d3dx12.h"

TextureProxy::TextureProxy(Resource::Type Type)
	: ResourceProxy(Type)
{
	m_Format			= DXGI_FORMAT_UNKNOWN;
	m_Width				= 0;
	m_Height			= 0;
	m_DepthOrArraySize	= 1;
	m_MipLevels			= 1;
}

void TextureProxy::SetFormat(DXGI_FORMAT Format)
{
	m_Format = Format;
}

void TextureProxy::SetWidth(UINT64 Width)
{
	m_Width = Width;
}

void TextureProxy::SetHeight(UINT Height)
{
	m_Height = Height;
}

void TextureProxy::SetDepthOrArraySize(UINT16 DepthOrArraySize)
{
	m_DepthOrArraySize = DepthOrArraySize;
}

void TextureProxy::SetMipLevels(UINT16 MipLevels)
{
	m_MipLevels = MipLevels;
}

void TextureProxy::SetClearValue(D3D12_CLEAR_VALUE ClearValue)
{
	m_ClearValue = std::make_optional<D3D12_CLEAR_VALUE>(ClearValue);
}

void TextureProxy::Link()
{
	assert
	(
		m_Type == Resource::Type::Texture1D ||
		m_Type == Resource::Type::Texture2D ||
		m_Type == Resource::Type::Texture3D ||
		m_Type == Resource::Type::TextureCube
	);

	if (m_Type == Resource::Type::TextureCube)
		m_DepthOrArraySize = 6;

	const bool IsArray = (m_Type == Resource::Type::Texture1D || m_Type == Resource::Type::Texture2D || m_Type == Resource::Type::TextureCube) && m_DepthOrArraySize > 1;
	m_NumSubresources = IsArray ? m_DepthOrArraySize * m_MipLevels : m_MipLevels;

	ResourceProxy::Link();
}

D3D12_HEAP_PROPERTIES TextureProxy::BuildD3DHeapProperties() const
{
	return kDefaultHeapProps;
}

D3D12_RESOURCE_DESC TextureProxy::BuildD3DDesc() const
{
	const D3D12_RESOURCE_FLAGS ResourceFlags = GetD3DResourceFlags(BindFlags);
	switch (m_Type)
	{
	case Resource::Type::Texture1D:		return CD3DX12_RESOURCE_DESC::Tex1D(m_Format, m_Width, m_DepthOrArraySize, m_MipLevels, ResourceFlags);
	case Resource::Type::Texture2D:		return CD3DX12_RESOURCE_DESC::Tex2D(m_Format, m_Width, m_Height, m_DepthOrArraySize, m_MipLevels, 1, 0, ResourceFlags);
	case Resource::Type::Texture3D:		return CD3DX12_RESOURCE_DESC::Tex3D(m_Format, m_Width, m_Height, m_DepthOrArraySize, m_MipLevels, ResourceFlags);
	case Resource::Type::TextureCube:	return CD3DX12_RESOURCE_DESC::Tex2D(m_Format, m_Width, m_Height, m_DepthOrArraySize, m_MipLevels, 1, 0, ResourceFlags);
	default:							assert(false && __FUNCTION__); return D3D12_RESOURCE_DESC();
	}
}