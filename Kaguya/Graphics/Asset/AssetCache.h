#pragma once
#include <basetsd.h>
#include <compare>
#include <memory>
#include <vector>
#include <unordered_set>
#include <unordered_map>
#include <Core/Synchronization/RWLock.h>
#include <Core/Utility.h>

constexpr UINT64 MAX_UID = (1ull << 32ull);
constexpr UINT64 MAX_PTR = (1ull << 32ull);

struct AssetHandle
{
	AssetHandle()
	{
		UID = MAX_UID;
		Ptr = MAX_PTR;
	}

	auto operator<=>(const AssetHandle&) const = default;

	operator bool() const
	{
		return UID != MAX_UID && Ptr != MAX_PTR;
	}

	UINT64 UID : 32ull;
	UINT64 Ptr : 32ull;
};

static_assert(sizeof(AssetHandle) == sizeof(UINT64));

namespace std
{
	template<>
	struct hash<AssetHandle>
	{
		size_t operator()(const AssetHandle& AssetHandle) const noexcept
		{
			size_t seed = 0;
			hash_combine(seed, size_t(AssetHandle.UID));
			hash_combine(seed, size_t(AssetHandle.Ptr));
			return seed;
		}
	};
}

template<typename T>
class AssetCache
{
public:
	AssetCache()
	{
		UniqueIdentifier = 0;
	}

	auto begin()
	{
		return AssetHashTable.begin();
	}

	auto end()
	{
		return AssetHashTable.end();
	}

	template<typename... Args>
	void Create(const std::string& Name, Args... args)
	{
		std::shared_ptr<T> Asset = std::make_shared<T>(std::forward<Args>(args)...);

		ScopedWriteLock SWL(RWLock);
		AssetHandle Handle;
		Handle.UID = UniqueIdentifier++;
		Handle.Ptr = Assets.size();

		Assets.push_back(std::move(Asset));
		Handles.insert(Handle);
		AssetHashTable[Name] = Handle;
	}

	AssetHandle GetAssetHandle(const std::string& Name)
	{
		ScopedReadLock SRL(RWLock);

		if (auto iter = AssetHashTable.find(Name);
			iter != AssetHashTable.end())
		{
			return iter->second;
		}

		return AssetHandle();
	}

	std::shared_ptr<T> GetResource(AssetHandle Handle)
	{
		ScopedReadLock SRL(RWLock);

		if (auto iter = Handles.find(Handle);
			iter != Handles.end())
		{
			return Assets[iter->Ptr];
		}

		return {};
	}

	bool Exist(const std::string& Name)
	{
		ScopedReadLock SRL(RWLock);

		auto iter = AssetHashTable.find(Name);
		return iter != AssetHashTable.end();
	}
private:
	RWLock RWLock;
	UINT64 UniqueIdentifier;
	std::vector<std::shared_ptr<T>> Assets;
	std::unordered_set<AssetHandle> Handles;
	std::unordered_map<std::string, AssetHandle> AssetHashTable;

	friend class AssetWindow;
};