#pragma once
#include "Scene.h"
#include "RTScene.h"

class ResourceManager;

class Editor
{
public:
	void SetContext(ResourceManager* pResourceManager)
	{
		this->pResourceManager = pResourceManager;
	}

private:
	ResourceManager* pResourceManager;
};