#pragma once
#include <filesystem>
#include "Scene.h"

class SceneSerializer
{
public:
	SceneSerializer(Scene* pScene)
		: pScene(pScene)
	{

	}

	void Serialize(const std::filesystem::path& Path);

	Scene Deserialize(const std::filesystem::path& Path);
private:
	Scene* pScene;
};