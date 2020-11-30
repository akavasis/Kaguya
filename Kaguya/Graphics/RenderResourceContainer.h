#pragma once
#include <Core/Synchronization/RWLock.h>
#include <unordered_map>
#include "RenderResourceHandle.h"

template <typename T>
class RenderResourceContainer
{
public:
	template<typename... Args>
	T* CreateResource(RenderResourceHandle RenderResourceHandle, Args&&... args)
	{
		if (!RenderResourceHandle)
			return nullptr;

		T resource = T(std::forward<Args>(args)...);

		ScopedWriteLock lk(m_RWLock);
		m_ResourceTable.emplace(RenderResourceHandle, std::move(resource));
		return &m_ResourceTable[RenderResourceHandle];
	}

	// Returns null if not found
	T* GetResource(RenderResourceHandle RenderResourceHandle)
	{
		ScopedReadLock lk(m_RWLock);
		auto iter = m_ResourceTable.find(RenderResourceHandle);
		if (iter != m_ResourceTable.end())
			return &iter->second;
		return nullptr;
	}

	// Returns null if not found
	const T* GetResource(RenderResourceHandle RenderResourceHandle) const
	{
		ScopedReadLock lk(m_RWLock);
		auto iter = m_ResourceTable.find(RenderResourceHandle);
		if (iter != m_ResourceTable.end())
			return &iter->second;
		return nullptr;
	}

	// Returns true if it is found and destroyed
	bool Destroy(RenderResourceHandle RenderResourceHandle)
	{
		ScopedWriteLock lk(m_RWLock);
		auto iter = m_ResourceTable.find(RenderResourceHandle);
		if (iter != m_ResourceTable.end())
		{
			m_ResourceTable.erase(iter);
			return true;
		}
		return false;
	}
private:
	RWLock m_RWLock;
	std::unordered_map<RenderResourceHandle, T> m_ResourceTable;
};