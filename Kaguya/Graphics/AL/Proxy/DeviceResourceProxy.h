#pragma once
#include <d3d12.h>
#include <optional>
#include "Proxy.h"
#include "../D3D12/DeviceResource.h"

class DeviceResourceProxy : public Proxy
{
public:
	friend class DeviceResource;
	DeviceResourceProxy(DeviceResource::Type Type);

	virtual D3D12_HEAP_PROPERTIES BuildD3DHeapProperties() const = 0;
	virtual D3D12_RESOURCE_DESC BuildD3DDesc() const = 0;

	DeviceResource::BindFlags			BindFlags;			//< Default value: None
	DeviceResource::State				InitialState;		//< Default value: Unknown
protected:
	void Link() override;

	DeviceResource::Type				m_Type;				//< Default value: constructor value
	UINT								m_NumSubresources;	//< Default value: 0, this is set automatically
	std::optional<D3D12_CLEAR_VALUE>	m_ClearValue;		//< Default value: std::nullopt
};