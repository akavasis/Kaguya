#pragma once
#include <unordered_map>
#include <filesystem>

#include "Model.h"

class ModelLoader
{
public:
	ModelLoader(std::filesystem::path ExecutableFolderPath);
	virtual ~ModelLoader();

	[[nodiscard]] Model LoadFromFile(const char* pPath, float Scale = 1.0f);
private:
	std::filesystem::path m_ExecutableFolderPath;
};