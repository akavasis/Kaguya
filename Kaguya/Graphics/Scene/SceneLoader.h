#pragma once
#include <filesystem>

#include "Scene.h"

class SceneLoader
{
public:
	SceneLoader(std::filesystem::path ExecutableFolderPath);

	[[nodiscard]] Scene LoadFromFile(const char* pPath, float Scale = 1.0f) const;
private:
	std::filesystem::path m_ExecutableFolderPath;
};