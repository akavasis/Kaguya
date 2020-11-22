#pragma once
#include <Core/Synchronization/RWLock.h>
#include <atomic>
#include <unordered_map>
#include <queue>
#include "RenderResourceHandle.h"

template <RenderResourceType Type, typename T>
class RenderResourceContainer
{
public:
	template<typename... Args>
	std::pair<RenderResourceHandle, T*> CreateResource(Args&&... args)
	{
		T resource = T(std::forward<Args>(args)...);

		ScopedWriteLock lk(m_RWLock);
		RenderResourceHandle handle;
		if (!m_AvailableHandles.empty())
		{
			handle = m_AvailableHandles.front();
			m_AvailableHandles.pop();
		}
		else
		{
			handle = m_HandleFactory.GetNewHandle();
		}

		m_ResourceTable.emplace(handle, std::move(resource));
		return { handle, &m_ResourceTable[handle] };
	}

	// Returns null if not found
	T* GetResource(const RenderResourceHandle& RenderResourceHandle)
	{
		ScopedReadLock lk(m_RWLock);
		auto iter = m_ResourceTable.find(RenderResourceHandle);
		if (iter != m_ResourceTable.end())
			return &iter->second;
		return nullptr;
	}

	// Returns null if not found
	const T* GetResource(const RenderResourceHandle& RenderResourceHandle) const
	{
		ScopedReadLock lk(m_RWLock);
		auto iter = m_ResourceTable.find(RenderResourceHandle);
		if (iter != m_ResourceTable.end())
			return &iter->second;
		return nullptr;
	}

	// Returns true if it is found and destroyed
	bool Destroy(RenderResourceHandle* pRenderResourceHandle)
	{
		if (!pRenderResourceHandle)
			return false;

		ScopedWriteLock lk(m_RWLock);
		auto iter = m_ResourceTable.find(*pRenderResourceHandle);
		if (iter != m_ResourceTable.end())
		{
			m_ResourceTable.erase(iter);
			m_AvailableHandles.push(*pRenderResourceHandle);

			pRenderResourceHandle->Type = RenderResourceType::Unknown;
			pRenderResourceHandle->Flags = RenderResourceFlags::Destroyed;
			pRenderResourceHandle->Data = 0;
			return true;
		}
		return false;
	}
private:
	template<RenderResourceType Type>
	class RenderResourceHandleFactory
	{
	public:
		RenderResourceHandleFactory()
		{
			m_AtomicData = 0;
		}

		RenderResourceHandle GetNewHandle()
		{
			RenderResourceHandle Handle;
			Handle.Type = Type;
			Handle.Flags = RenderResourceFlags::Active;
			Handle.Data = m_AtomicData.fetch_add(1);

			return Handle;
		}
	private:
		std::atomic<size_t> m_AtomicData;
	};

	RWLock m_RWLock;
	std::unordered_map<RenderResourceHandle, T> m_ResourceTable;
	RenderResourceHandleFactory<Type> m_HandleFactory;
	std::queue<RenderResourceHandle> m_AvailableHandles;
};