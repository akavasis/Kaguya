#pragma once
#include <d3d12.h>

#include <unordered_map>
#include <optional>

class ResourceStateTracker
{
public:
	using ResourceType = ID3D12Resource;

	struct ResourceState
	{
		D3D12_RESOURCE_STATES State;
		std::unordered_map<UINT, D3D12_RESOURCE_STATES> SubresourceState;

		D3D12_RESOURCE_STATES GetSubresourceState(UINT Subresource) const
		{
			const auto iter = SubresourceState.find(Subresource);
			if (iter != SubresourceState.end())
				return iter->second;
			return State;
		}
		void SetSubresourceState(UINT Subresource, D3D12_RESOURCE_STATES State)
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
	};

	void AddResourceState(ResourceType* pResource, D3D12_RESOURCE_STATES ResourceStates);
	void RemoveResourceState(ResourceType* pResource);
	void SetResourceState(ResourceType* pResource, UINT Subresource, D3D12_RESOURCE_STATES ResourceStates);
	void UpdateResourceStates(const ResourceStateTracker& RST);
	void Reset();

	UINT FlushPendingResourceBarriers(const ResourceStateTracker& GlobalResourceStateTracker, ID3D12GraphicsCommandList* pCommandList);
	UINT FlushResourceBarriers(ID3D12GraphicsCommandList* pCommandList);

	void ResourceBarrier(const D3D12_RESOURCE_BARRIER& Barrier);

	std::optional<ResourceStateTracker::ResourceState> Find(ResourceType* pResource) const;
private:
	std::unordered_map<ResourceType*, ResourceState> m_ResourceStates;

	// Pending resource transitions are committed to a separate commandlist before this commandlist
	// is executed on the command queue. This guarantees that resources will
	// be in the expected state at the beginning of a command list.
	std::vector<D3D12_RESOURCE_BARRIER> m_PendingResourceBarriers;
	// Resource barriers that need to be committed to the command list.
	std::vector<D3D12_RESOURCE_BARRIER> m_ResourceBarriers;
};