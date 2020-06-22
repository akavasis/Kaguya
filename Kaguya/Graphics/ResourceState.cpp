#include "pch.h"
#include "ResourceState.h"

ResourceState::ResourceState(D3D12_RESOURCE_STATES State)
	: State(State)
{
}

ResourceState::~ResourceState()
{
}

D3D12_RESOURCE_STATES ResourceState::GetSubresourceState(UINT Subresource) const
{
	const auto iter = SubresourceState.find(Subresource);
	if (iter != SubresourceState.end())
	{
		return iter->second;
	}
	return State;
}

void ResourceState::SetSubresourceState(UINT Subresource, D3D12_RESOURCE_STATES State)
{
	if (Subresource == D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES)
	{
		this->State = State;
		SubresourceState.clear();
	}
	else
	{
		SubresourceState[Subresource] = State;
	}
}