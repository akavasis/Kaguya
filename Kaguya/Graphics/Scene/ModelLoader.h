#pragma once
#include <filesystem>

#include "Model.h"

class ModelLoader
{
public:
	ModelLoader(std::filesystem::path ExecutableFolderPath);

	[[nodiscard]] Model LoadFromFile(const std::filesystem::path& Path) const;
private:
	std::filesystem::path m_ExecutableFolderPath;
};