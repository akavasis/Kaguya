#pragma once
#include <unordered_map>
#include <optional>

#include "Resource.h"

class ResourceStateTracker
{
public:
	struct ResourceState
	{
		ResourceStates State;
		std::unordered_map<UINT, ResourceStates> SubresourceState;

		ResourceState(ResourceStates State = ResourceStates::Common);

		ResourceStates GetSubresourceState(UINT Subresource) const;
		void SetSubresourceState(UINT Subresource, ResourceStates State);
	};

	void AddResourceState(Resource* pResource, ResourceStates ResourceStates);
	void RemoveResourceState(Resource* pResource);
	void SetResourceState(Resource* pResource, UINT Subresource, ResourceStates ResourceStates);
	void UpdateResourceStates(const ResourceStateTracker& RST);
	void Reset();

	std::optional<ResourceStateTracker::ResourceState> Find(Resource* pResource) const;
private:
	std::unordered_map<Resource*, ResourceState> m_ResourceStates;
};