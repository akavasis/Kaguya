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

		ResourceState(D3D12_RESOURCE_STATES State = D3D12_RESOURCE_STATES::D3D12_RESOURCE_STATE_COMMON)
			: State(State)
		{
		}

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

	std::optional<ResourceStateTracker::ResourceState> Find(ResourceType* pResource) const;
private:
	std::unordered_map<ResourceType*, ResourceState> m_ResourceStates;
};