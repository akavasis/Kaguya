#pragma once
#include "DeviceResource.h"

class Device;
class DeviceTextureProxy;

class DeviceTexture : public DeviceResource
{
public:
	DeviceTexture() = default;
	DeviceTexture(Microsoft::WRL::ComPtr<ID3D12Resource> ExistingID3D12Resource);
	DeviceTexture(const Device* pDevice, DeviceTextureProxy& Proxy);
	DeviceTexture(const Device* pDevice, const Heap* pHeap, UINT64 HeapOffset, DeviceTextureProxy& Proxy);
	~DeviceTexture() override;

	inline auto GetFormat() const { return m_Format; }
	inline auto GetWidth() const { return m_Width; }
	inline auto GetHeight() const { return m_Height; }
	inline auto GetDepthOrArraySize() const { return m_DepthOrArraySize; }
	inline auto GetMipLevels() const { return m_MipLevels; }

	bool IsArray() const;
private:
	DXGI_FORMAT m_Format;
	UINT64		m_Width;
	UINT		m_Height;
	UINT16		m_DepthOrArraySize;
	UINT16		m_MipLevels;
};