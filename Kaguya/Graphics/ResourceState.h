#pragma once
#include <d3d12.h>

#include <unordered_map>

struct ResourceState
{
	D3D12_RESOURCE_STATES State;
	std::unordered_map<UINT, D3D12_RESOURCE_STATES> SubresourceState;

	ResourceState(D3D12_RESOURCE_STATES State = D3D12_RESOURCE_STATE_COMMON);
	~ResourceState();

	D3D12_RESOURCE_STATES GetSubresourceState(UINT Subresource) const;
	void SetSubresourceState(UINT Subresource, D3D12_RESOURCE_STATES State);
};