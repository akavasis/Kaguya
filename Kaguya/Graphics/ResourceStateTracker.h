#pragma once
#include <d3d12.h>

#include <unordered_map>
#include <optional>

#include "ResourceState.h"

class ResourceStateTracker
{
public:
	ResourceStateTracker();
	~ResourceStateTracker();

	void AddResourceState(ID3D12Resource* pD3D12Resource, D3D12_RESOURCE_STATES ResourceState);
	void RemoveResourceState(ID3D12Resource* pD3D12Resource);
	void SetResourceState(ID3D12Resource* pD3D12Resource, UINT Subresource, D3D12_RESOURCE_STATES ResourceState);
	void UpdateResourceStates(const ResourceStateTracker& RST);
	void Reset();

	std::optional<ResourceState> Find(ID3D12Resource* pD3D12Resource) const;
private:
	std::unordered_map<ID3D12Resource*, ResourceState> m_ResourceStates;
};