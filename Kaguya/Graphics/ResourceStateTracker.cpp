#include "pch.h"
#include "ResourceStateTracker.h"

ResourceStateTracker::ResourceStateTracker()
{
}

ResourceStateTracker::~ResourceStateTracker()
{
}

void ResourceStateTracker::AddResourceState(ID3D12Resource* pD3D12Resource, D3D12_RESOURCE_STATES ResourceState)
{
	if (!pD3D12Resource) return;
	m_ResourceStates[pD3D12Resource].SetSubresourceState(D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES, ResourceState);
}

void ResourceStateTracker::RemoveResourceState(ID3D12Resource* pD3D12Resource)
{
	if (!pD3D12Resource) return;
	m_ResourceStates.erase(pD3D12Resource);
}

void ResourceStateTracker::SetResourceState(ID3D12Resource* pD3D12Resource, UINT Subresource, D3D12_RESOURCE_STATES ResourceState)
{
	if (!pD3D12Resource) return;
	m_ResourceStates[pD3D12Resource].SetSubresourceState(Subresource, ResourceState);
}

void ResourceStateTracker::UpdateResourceStates(const ResourceStateTracker& RST)
{
	for (const auto& resourceState : RST.m_ResourceStates)
	{
		m_ResourceStates[resourceState.first] = resourceState.second;
	}
}

void ResourceStateTracker::Reset()
{
	m_ResourceStates.clear();
}

std::optional<ResourceState> ResourceStateTracker::Find(ID3D12Resource* pD3D12Resource) const
{
	auto iter = m_ResourceStates.find(pD3D12Resource);
	if (iter != m_ResourceStates.end())
		return iter->second;
	return {};
}