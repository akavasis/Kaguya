#pragma once
#include <Core/Synchronization/RWLock.h>
#include "RenderDevice.h"

#include "Asset/AsyncLoader.h"

class AssetManager
{
public:
	static void Initialize();
	static void Shutdown();
	static AssetManager& Instance();

	Descriptor GetDefaultWhiteTexture() { return SystemTextureSRVs[AssetTextures::DefaultWhite]; }
	Descriptor GetDefaultBlackTexture() { return SystemTextureSRVs[AssetTextures::DefaultBlack]; }
	Descriptor GetDefaultAlbedoTexture() { return SystemTextureSRVs[AssetTextures::DefaultAlbedo]; }
	Descriptor GetDefaultNormalTexture() { return SystemTextureSRVs[AssetTextures::DefaultNormal]; }
	Descriptor GetDefaultRoughnessTexture() { return SystemTextureSRVs[AssetTextures::DefaultRoughness]; }

	auto& GetImageCache()
	{
		return ImageCache;
	}

	auto& GetMeshCache()
	{
		return MeshCache;
	}

	void AsyncLoadImage(const std::filesystem::path& Path, bool sRGB);
	void AsyncLoadMesh(const std::filesystem::path& Path, bool KeepGeometryInRAM);
private:
	AssetManager();
	AssetManager(const AssetManager&) = delete;
	AssetManager& operator=(const AssetManager&) = delete;
	~AssetManager();

	void CreateSystemTextures();

	static DWORD WINAPI ResourceUploadThreadProc(_In_ PVOID pParameter);
private:
	struct AssetTextures
	{
		// These textures can be created using small resources
		enum System
		{
			DefaultWhite,
			DefaultBlack,
			DefaultAlbedo,
			DefaultNormal,
			DefaultRoughness,
			NumSystemTextures
		};
	};

	Microsoft::WRL::ComPtr<ID3D12Heap> SystemTextureHeap;
	Microsoft::WRL::ComPtr<ID3D12Resource> SystemTextures[AssetTextures::NumSystemTextures];
	Descriptor SystemTextureSRVs[AssetTextures::NumSystemTextures];

	AsyncImageLoader AsyncImageLoader;
	AsyncMeshLoader AsyncMeshLoader;

	AssetCache<Asset::Image> ImageCache;
	AssetCache<Asset::Mesh> MeshCache;

	CriticalSection UploadCriticalSection;
	ConditionVariable UploadConditionVariable;
	ThreadSafeQueue<std::shared_ptr<Asset::Image>> ImageUploadQueue;
	ThreadSafeQueue<std::shared_ptr<Asset::Mesh>> MeshUploadQueue;

	wil::unique_handle Thread;
	std::atomic<bool> ShutdownThread = false;

	friend class AssetWindow;
};