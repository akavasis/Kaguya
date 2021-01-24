#pragma once
#include <filesystem>
#include "Scene.h"

class SceneSerializer
{
public:
	static void Serialize(const std::filesystem::path& Path, Scene* pScene);

	static [[nodiscard]] Scene Deserialize(const std::filesystem::path& Path);
};