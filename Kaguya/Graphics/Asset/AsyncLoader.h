#pragma once
#include <wil/resource.h>

#include "Image.h"
#include "Mesh.h"

#include "AssetCache.h"

/*
*	All inherited loader must implement a method called AsyncLoad,
*	it takes in the TMetadata and returns a TResourcePtr
*/
template<typename T, typename Metadata, typename Loader>
class AsyncLoader
{
public:
	using TResource = T;
	using TResourcePtr = std::shared_ptr<TResource>;
	using TMetadata = Metadata;
	using TDelegate = std::function<void(TResourcePtr)>;

	AsyncLoader()
	{
		Thread.reset(::CreateThread(nullptr, 0, &AsyncThreadProc, this, 0, nullptr));
	}

	~AsyncLoader()
	{
		Shutdown = true;
		ConditionVariable.WakeAll();

		::WaitForSingleObject(Thread.get(), INFINITE);
	}

	void RequestAsyncLoad(UINT NumMetadata, Metadata* pMetadata, TDelegate Delegate)
	{
		ScopedCriticalSection SCS(CriticalSection);
		this->Delegate = std::move(Delegate);
		for (UINT i = 0; i < NumMetadata; i++)
		{
			MetadataQueue.Enqueue(pMetadata[i]);
		}
		ConditionVariable.Wake();
	}
private:
	static DWORD WINAPI AsyncThreadProc(_In_ PVOID pParameter)
	{
		auto pAsyncLoader = static_cast<AsyncLoader<T, Metadata, Loader>*>(pParameter);

		while (true)
		{
			// Sleep until the condition variable becomes notified
			ScopedCriticalSection SCS(pAsyncLoader->CriticalSection);
			pAsyncLoader->ConditionVariable.Wait(pAsyncLoader->CriticalSection, INFINITE);

			if (pAsyncLoader->Shutdown)
			{
				break;
			}

			// Start loading stuff async :D
			TMetadata Item = {};
			while (pAsyncLoader->MetadataQueue.Dequeue(Item, 0))
			{
				auto pResource = static_cast<Loader*>(pAsyncLoader)->AsyncLoad(Item);
				if (pResource && pAsyncLoader->Delegate)
				{
					pAsyncLoader->Delegate(pResource);
				}
			}
		}

		return EXIT_SUCCESS;
	}
private:
	CriticalSection CriticalSection;
	ConditionVariable ConditionVariable;
	ThreadSafeQueue<Metadata> MetadataQueue;
	TDelegate Delegate = nullptr;

	wil::unique_handle Thread;
	std::atomic<bool> Shutdown = false;
};

class AsyncImageLoader : public AsyncLoader<Asset::Image, Asset::ImageMetadata, AsyncImageLoader>
{
public:
	TResourcePtr AsyncLoad(const TMetadata& Metadata);
};

class AsyncMeshLoader : public AsyncLoader<Asset::Mesh, Asset::MeshMetadata, AsyncMeshLoader>
{
public:
	TResourcePtr AsyncLoad(const TMetadata& Metadata);
};