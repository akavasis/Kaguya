#pragma once
#include <basetsd.h>
#include <memory>
#include <type_traits>
#include <unordered_map>

#include <Core/Synchronization/RWLock.h>
#include <Core/Utility.h>

template<typename T>
class AssetCache;

template<typename T>
class AssetHandle
{
	friend class AssetCache<T>;

	AssetHandle(std::shared_ptr<T> Resource)
		: Resource(std::move(Resource))
	{

	}
public:
	AssetHandle() = default;

	auto operator<=>(const AssetHandle&) const = default;

	explicit operator bool() const
	{
		return static_cast<bool>(Resource);
	}

	[[nodiscard]] const T& Get() const
	{
		return *Resource;
	}

	[[nodiscard]] T& Get()
	{
		return *Resource;
	}

	[[nodiscard]] const T& operator*() const
	{
		return Get();
	}

	[[nodiscard]] T& operator*()
	{
		return Get();
	}

	T* operator->() const
	{
		return Resource.get();
	}

	T* operator->()
	{
		return Resource.get();
	}
private:
	std::shared_ptr<T> Resource;
};

template<typename T>
class AssetCache
{
public:
	using Handle = AssetHandle<T>;

	AssetCache() = default;

	AssetCache(AssetCache&&) = default;

	AssetCache& operator=(AssetCache&&) = default;

	void DestroyAll()
	{
		ScopedWriteLock SWL(RWLock);

		Cache.clear();
	}

	template<typename... Args>
	void Create(UINT64 Key, Args&&... args)
	{
		ScopedWriteLock SWL(RWLock);

		if (Cache.find(Key) == Cache.end())
		{
			Cache[Key] = std::make_shared<T>(std::forward<Args>(args)...);
		}
	}

	void Discard(UINT64 Key)
	{
		ScopedWriteLock SWL(RWLock);

		if (auto it = Cache.find(Key);
			it != Cache.end())
		{
			Cache.erase(it);
		}
	}

	template<typename... Args>
	Handle Load(UINT64 Key, Args&&... args) const
	{
		ScopedReadLock SWL(RWLock);

		Handle Handle = {};
		if (auto it = Cache.find(Key);
			it != Cache.end())
		{
			Handle = it->second;
		}

		return Handle;
	}

	bool Exist(UINT64 Key) const
	{
		ScopedReadLock SRL(RWLock);

		return Cache.find(Key) != Cache.end();
	}

	template<typename Functor>
	void Each(Functor F) const
	{
		auto begin = Cache.begin();
		auto end = Cache.end();
		while (begin != end)
		{
			auto current = begin++;

			if constexpr (std::is_invocable_v<Functor, UINT64>)
			{
				F(current->first);
			}
			else if constexpr (std::is_invocable_v<Functor, Handle>)
			{
				F(Handle(current->second));
			}
			else
			{
				F(current->first, Handle(current->second));
			}
		}
	}

	// Acquires RWLock
	template<typename Functor>
	void Each_ThreadSafe(Functor F) const
	{
		ScopedWriteLock SWL(RWLock);
		auto begin = Cache.begin();
		auto end = Cache.end();
		while (begin != end)
		{
			auto current = begin++;

			if constexpr (std::is_invocable_v<Functor, UINT64>)
			{
				F(current->first);
			}
			else if constexpr (std::is_invocable_v<Functor, Handle>)
			{
				F(Handle(current->second));
			}
			else
			{
				F(current->first, Handle(current->second));
			}
		}
	}
private:
	mutable RWLock RWLock;
	std::unordered_map<UINT64, std::shared_ptr<T>> Cache;

	friend class AssetWindow;
	friend class SceneParser;
};