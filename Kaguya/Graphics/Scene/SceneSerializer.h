#pragma once
#include <filesystem>
#include "Scene.h"

class ResourceManager;

class SceneSerializer
{
public:
	static void Serialize(const std::filesystem::path& Path, Scene* pScene);

	static [[nodiscard]] void Deserialize(const std::filesystem::path& Path, Scene* pScene, ResourceManager* pResourceManager);
};