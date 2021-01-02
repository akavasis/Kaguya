#pragma once
#include <filesystem>

#include "Scene.h"

class SceneLoader
{
public:
	SceneLoader(std::filesystem::path ExecutableFolderPath);

	[[nodiscard]] Scene LoadFromFile(const std::filesystem::path& Path) const;
private:
	std::filesystem::path m_ExecutableFolderPath;
};