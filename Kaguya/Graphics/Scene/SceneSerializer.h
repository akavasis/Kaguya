#pragma once
#include <filesystem>

struct Scene;

class SceneSerializer
{
public:
	static void Serialize(const std::filesystem::path& Path, Scene* pScene);

	static void Deserialize(const std::filesystem::path& Path, Scene* pScene);
};