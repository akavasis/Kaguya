#pragma once
#include <filesystem>
#include "Scene/Material.h"

#include "RenderEngine.h"

class MaterialLoader
{
public:
	MaterialLoader(RenderEngine* pRenderEngine, std::filesystem::path ExecutableFolder);

	Material LoadMaterial(const char* pAlbedoMapPath,
		const char* pNormalMapPath,
		const char* pRoughnessMapPath,
		const char* pMetallicMapPath,
		const char* pEmissiveMapPath);

private:
	RenderEngine* m_pRenderEngine;
	std::vector<RenderResourceHandle> m_Textures;
};