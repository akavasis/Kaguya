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

	void SetSelectedEntity(Entity Entity) { SelectedEntity = Entity; }
private:
	ResourceManager* pResourceManager = nullptr;
	Scene* pScene = nullptr;
	Entity SelectedEntity = {};
};

class InspectorWindow
{
public:
	void SetContext(ResourceManager* pResourceManager, Scene* pScene, Entity Entity)
	{
		this->pResourceManager = pResourceManager;
		this->pScene = pScene;
		SelectedEntity = Entity;
	}

	void RenderGui();
private:
	ResourceManager* pResourceManager = nullptr;
	Scene* pScene;
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
		this->pScene = pScene;
		HierarchyWindow.SetContext(pResourceManager, pScene);
		InspectorWindow.SetContext(pResourceManager, pScene, {});
		AssetWindow.SetContext(pResourceManager);
	}

	void RenderGui();
public:
	HierarchyWindow HierarchyWindow;
	InspectorWindow InspectorWindow;
	AssetWindow AssetWindow;
private:
	ResourceManager* pResourceManager;
	Scene* pScene;
};