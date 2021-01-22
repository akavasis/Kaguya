#pragma once
#include "Scene/Scene.h"
#include "Scene/Entity.h"

#include "Camera.h"

#include "RTScene.h"

class ResourceManager;

class HierarchyWindow
{
public:
	void SetContext(Scene* pScene)
	{
		this->pScene = pScene;
		SelectedEntity = {};
	}

	void RenderGui();

	Entity GetSelectedEntity() const { return SelectedEntity; }
private:
	Scene* pScene;
	Entity SelectedEntity;
};

class InspectorWindow
{
public:
	void SetContext(Entity Entity);

	void RenderGui();
private:
	Entity SelectedEntity;
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
		HierarchyWindow.SetContext(pScene);
		InspectorWindow.SetContext({});
		AssetWindow.SetContext(pResourceManager);
	}

	void RenderGui();
private:
	ResourceManager* pResourceManager;

	HierarchyWindow HierarchyWindow;
	InspectorWindow InspectorWindow;
	AssetWindow AssetWindow;
};