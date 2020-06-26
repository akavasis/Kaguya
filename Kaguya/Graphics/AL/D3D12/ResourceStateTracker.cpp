#include "pch.h"
#include "ResourceStateTracker.h"

void ResourceStateTracker::AddResourceState(ResourceType* pResource, D3D12_RESOURCE_STATES ResourceStates)
{
	if (!pResource) return;
	m_ResourceStates[pResource].SetSubresourceState(D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES, ResourceStates);
}

void ResourceStateTracker::RemoveResourceState(ResourceType* pResource)
{
	if (!pResource) return;
	m_ResourceStates.erase(pResource);
}

void ResourceStateTracker::SetResourceState(ResourceType* pResource, UINT Subresource, D3D12_RESOURCE_STATES ResourceStates)
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

std::optional<ResourceStateTracker::ResourceState> ResourceStateTracker::Find(ResourceType* pResource) const
{
	auto iter = m_ResourceStates.find(pResource);
	if (iter != m_ResourceStates.end())
		return iter->second;
	return {};
}