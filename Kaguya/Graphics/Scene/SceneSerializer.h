#pragma once
#include <filesystem>
#include "Scene.h"

class SceneSerializer
{
public:
	static void Serialize(const std::filesystem::path& Path, Scene* pScene);

	static Scene Deserialize(const std::filesystem::path& Path);
};