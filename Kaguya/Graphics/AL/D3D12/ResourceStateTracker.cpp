#include "pch.h"
#include "ResourceStateTracker.h"

ResourceStateTracker::ResourceState::ResourceState(ResourceStates State)
	: State(State)
{
}

ResourceStates ResourceStateTracker::ResourceState::GetSubresourceState(UINT Subresource) const
{
	const auto iter = SubresourceState.find(Subresource);
	if (iter != SubresourceState.end())
		return iter->second;
	return State;
}

void ResourceStateTracker::ResourceState::SetSubresourceState(UINT Subresource, ResourceStates ResourceStates)
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

void ResourceStateTracker::AddResourceState(Resource* pResource, ResourceStates ResourceStates)
{
	if (!pResource) return;
	m_ResourceStates[pResource].SetSubresourceState(D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES, ResourceStates);
}

void ResourceStateTracker::RemoveResourceState(Resource* pResource)
{
	if (!pResource) return;
	m_ResourceStates.erase(pResource);
}

void ResourceStateTracker::SetResourceState(Resource* pResource, UINT Subresource, ResourceStates ResourceStates)
{
	if (!pResource) return;
	m_ResourceStates[pResource].SetSubresourceState(Subresource, ResourceStates);
}

void ResourceStateTracker::UpdateResourceStates(const ResourceStateTracker& RST)
{
	Reset();
	for (const auto& resourceState : RST.m_ResourceStates)
	{
		m_ResourceStates[resourceState.first] = resourceState.second;
	}
}

void ResourceStateTracker::Reset()
{
	m_ResourceStates.clear();
}

std::optional<ResourceStateTracker::ResourceState> ResourceStateTracker::Find(Resource* pResource) const
{
	auto iter = m_ResourceStates.find(pResource);
	if (iter != m_ResourceStates.end())
		return iter->second;
	return {};
}