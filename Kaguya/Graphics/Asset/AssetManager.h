#pragma once
#include <wil/resource.h>

#include "Image.h"
#include "Mesh.h"

#include "AssetCache.h"

using ImageCallback = std::function<void(std::shared_ptr<Asset::Image>)>;
using MeshCallback = std::function<void(std::shared_ptr<Mesh>)>;

class AssetManager
{
public:
	AssetManager();
	~AssetManager();

	std::string GetName(const std::filesystem::path& Path);

	void AsyncLoadImage(const std::filesystem::path& Path, bool sRGB, ImageCallback Callback);
	void AsyncLoadMesh(const std::filesystem::path& Path, bool KeepGeometryInRAM, MeshCallback Callback);
private:
	static DWORD WINAPI AssetStreamThreadProc(_In_ PVOID pParameter);
private:
	AssetCache<Asset::Image> ImageCache;
	AssetCache<Mesh> MeshCache;

	CriticalSection AssetStreamCriticalSection;
	ConditionVariable AssetStreamConditionVariable;

	wil::unique_handle Thread;
	std::atomic<bool> Shutdown = false;

	friend class ResourceManager;
	friend class AssetWindow;
};