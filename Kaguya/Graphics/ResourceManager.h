#pragma once
#include <Core/Synchronization/RWLock.h>
#include "RenderDevice.h"

#include "Asset/Mesh.h"

class ResourceManager
{
public:
	ResourceManager(RenderDevice* pRenderDevice);
	ResourceManager(const ResourceManager&) = delete;
	ResourceManager& operator=(const ResourceManager&) = delete;
	~ResourceManager();

	Descriptor GetDefaultWhiteTexture() { return SystemTextureSRVs[AssetTextures::DefaultWhite]; }
	Descriptor GetDefaultBlackTexture() { return SystemTextureSRVs[AssetTextures::DefaultBlack]; }
	Descriptor GetDefaultAlbedoTexture() { return SystemTextureSRVs[AssetTextures::DefaultAlbedo]; }
	Descriptor GetDefaultNormalTexture() { return SystemTextureSRVs[AssetTextures::DefaultNormal]; }
	Descriptor GetDefaultRoughnessTexture() { return SystemTextureSRVs[AssetTextures::DefaultRoughness]; }

	Descriptor GetTexture(const std::string& Name);

	void AsyncLoadTexture2D(const std::filesystem::path& Path, bool sRGB, std::function<void()> Callback = nullptr);
	void AsyncLoadMesh(const std::filesystem::path& Path, bool KeepGeometryInRAM, std::function<void()> Callback = nullptr);
private:
	void CreateSystemTextures();

	/*
		AsyncLoad methods will wake this thread to start producing the resources
	*/
	static DWORD WINAPI ResourceProducerThreadProc(_In_ PVOID pParameter);

	/*
		This thread consumes the resource produced by ResourceProducerThreadProc
		and creates Descriptor and GPU Resources
	*/
	static DWORD WINAPI ResourceConsumerThreadProc(_In_ PVOID pParameter);
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

	RenderDevice* pRenderDevice;

	Microsoft::WRL::ComPtr<ID3D12Heap> SystemTextureHeap;
	Microsoft::WRL::ComPtr<ID3D12Resource> SystemTextures[AssetTextures::NumSystemTextures];
	Descriptor SystemTextureSRVs[AssetTextures::NumSystemTextures];

	RWLock TextureCacheLock;
	std::unordered_map<std::string, Texture2D> TextureCache;

	RWLock MeshCacheLock;
	std::unordered_map<std::string, Mesh> MeshCache;

	wil::unique_handle ResourceThreads[2];
	std::atomic<bool> ExitResourceProcessThread = false;

	friend class AssetWindow;
};