#pragma once
#include "Scene/Scene.h"
#include "Scene/Entity.h"

#include "RaytracingAccelerationStructure.h"

class ResourceManager;

class HierarchyWindow
{
public:
	void SetContext(ResourceManager* pResourceManager, Scene* pScene)
	{
		this->pResourceManager = pResourceManager;
		this->pScene = pScene;
		SelectedEntity = {};
	}

	void RenderGui();

	Entity GetSelectedEntity() const { return SelectedEntity; }
private:
	ResourceManager* pResourceManager = nullptr;
	Scene* pScene = nullptr;
	Entity SelectedEntity = {};
};

class InspectorWindow
{
public:
	void SetContext(ResourceManager* pResourceManager, Entity Entity)
	{
		this->pResourceManager = pResourceManager;
		SelectedEntity = Entity;
	}

	void RenderGui();
private:
	ResourceManager* pResourceManager = nullptr;
	Entity SelectedEntity = {};
};

class AssetWindow
{
public:
	void SetContext(ResourceManager* pResourceManager)
	{
		this->pResourceManager = pResourceManager;
	}

	void RenderGui();
private:
	ResourceManager* pResourceManager;
};

class Editor
{
public:
	void SetContext(ResourceManager* pResourceManager, Scene* pScene)
	{
		this->pResourceManager = pResourceManager;
		HierarchyWindow.SetContext(pResourceManager, pScene);
		InspectorWindow.SetContext(pResourceManager, {});
		AssetWindow.SetContext(pResourceManager);
	}

	void RenderGui();
private:
	ResourceManager* pResourceManager;

	HierarchyWindow HierarchyWindow;
	InspectorWindow InspectorWindow;
	AssetWindow AssetWindow;
};