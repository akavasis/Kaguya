#pragma once
#include <d3d12.h>
#include <optional>
#include "Proxy.h"
#include "../D3D12/Resource.h"

class ResourceProxy : public Proxy
{
public:
	friend class Resource;
	ResourceProxy(Resource::Type Type);

	Resource::BindFlags	BindFlags;	//< Default value: None
	Resource::State	InitialState;	//< Default value: Unknown
protected:
	void Link() override;
	virtual D3D12_HEAP_PROPERTIES BuildD3DHeapProperties() const = 0;
	virtual D3D12_RESOURCE_DESC BuildD3DDesc() const = 0;

	Resource::Type m_Type;							//< Default value: constructor value
	UINT m_NumSubresources;							//< Default value: 0, this is set automatically
	std::optional<D3D12_CLEAR_VALUE> m_ClearValue;	//< Default value: std::nullopt
};