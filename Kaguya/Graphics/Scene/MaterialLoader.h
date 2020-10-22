#pragma once
#include <filesystem>

#include "Material.h"

class MaterialLoader
{
public:
	MaterialLoader(std::filesystem::path ExecutableFolderPath);

	[[nodiscard]] Material LoadMaterial(
		const char* pAlbedoMapPath,
		const char* pNormalMapPath,
		const char* pRoughnessMapPath,
		const char* pMetallicMapPath,
		const char* pEmissiveMapPath) const;
private:
	std::filesystem::path m_ExecutableFolderPath;
};