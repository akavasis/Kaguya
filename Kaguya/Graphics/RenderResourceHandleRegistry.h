#pragma once
#include <Core/Synchronization/RWLock.h>
#include <Template/Pool.h>
#include "RenderResourceHandle.h"

class RenderResourceHandleRegistry
{
public:
	enum
	{
		NumResources = 1024
	};

	RenderResourceHandle Allocate(RenderResourceType Type, const std::string& Name)
	{
		ScopedWriteLock SWL(RWLock);

		RenderResourceHandle Handle = {};
		Handle.Type = Type;
		Handle.UID = m_UIDCounter.fetch_add(1);
		Handle.Ptr = m_ResourcePool.Allocate();
		m_ResourcePool[Handle.Ptr].Value = Name;

		return Handle;
	}

	void Free(RenderResourceHandle Handle)
	{
		ScopedWriteLock SWL(RWLock);
		m_ResourcePool.Free(Handle.Ptr);
	}

	std::string GetName(RenderResourceHandle Handle)
	{
		ScopedReadLock SRL(RWLock);
		return m_ResourcePool[Handle.Ptr].Value;
	}
private:
	RWLock RWLock;
	std::atomic<UINT64> m_UIDCounter;
	Pool<std::string, NumResources> m_ResourcePool;
};