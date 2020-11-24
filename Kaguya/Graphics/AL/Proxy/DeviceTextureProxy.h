#pragma once
#include "DeviceResourceProxy.h"
#include "../D3D12/DeviceTexture.h"

class DeviceTextureProxy : public DeviceResourceProxy
{
public:
	friend class Texture;
	DeviceTextureProxy(Resource::Type Type);

	D3D12_HEAP_PROPERTIES BuildD3DHeapProperties() const override;
	D3D12_RESOURCE_DESC BuildD3DDesc() const override;

	void SetFormat(DXGI_FORMAT Format);
	void SetWidth(UINT64 Width);
	void SetHeight(UINT Height);
	void SetDepthOrArraySize(UINT16 DepthOrArraySize);
	void SetMipLevels(UINT16 MipLevels);
	void SetClearValue(D3D12_CLEAR_VALUE ClearValue);
protected:
	void Link() override;
private:
	DXGI_FORMAT m_Format;			// Default value: DXGI_FORMAT_UNKNOWN, must be set
	UINT64		m_Width;			// Default value: 0, must be set
	UINT		m_Height;			// Default value: 0, must be set
	UINT16		m_DepthOrArraySize;	// Default value: 1, optional set, if Type is TextureCube this value will be 6
	UINT16		m_MipLevels;		// Default value: 1, optional set
};