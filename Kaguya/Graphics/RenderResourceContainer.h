#pragma once
#include <atomic>
#include <mutex>
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
		std::scoped_lock lk(m_Mutex);

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
		auto iter = m_ResourceTable.find(RenderResourceHandle);
		if (iter != m_ResourceTable.end())
			return &iter->second;
		return nullptr;
	}

	// Returns null if not found
	const T* GetResource(const RenderResourceHandle& RenderResourceHandle) const
	{
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

		std::scoped_lock lk(m_Mutex);
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
			m_Handle.Type = Type;
			m_Handle.Flags = RenderResourceFlags::Active;
			m_Handle.Data = 0;
		}

		RenderResourceHandle GetNewHandle()
		{
			m_Handle.Data = m_AtomicData.fetch_add(1);
			return m_Handle;
		}
	private:
		RenderResourceHandle m_Handle;
		std::atomic<size_t> m_AtomicData;
	};

	std::mutex m_Mutex;
	std::unordered_map<RenderResourceHandle, T> m_ResourceTable;
	RenderResourceHandleFactory<Type> m_HandleFactory;
	std::queue<RenderResourceHandle> m_AvailableHandles;
};