#pragma once
#include "Scene/Scene.h"
#include "Scene/Entity.h"

#include "Camera.h"

#include "RTScene.h"

class ResourceManager;

class HierarchyPanel
{
public:
	void SetContext(Scene* pScene);

	void RenderGui();

	Entity GetSelectedEntity() const { return SelectedEntity; }
private:
	void RenderEntityGui(Entity entity);
private:
	Scene* pScene;
	Entity SelectedEntity;
};

class InspectorPanel
{
public:
	void SetContext(Entity Entity);

	void RenderGui();
private:
	Entity SelectedEntity;
};

class Editor
{
public:
	void SetContext(ResourceManager* pResourceManager, Scene* pScene)
	{
		this->pResourceManager = pResourceManager;
		HierarchyPanel.SetContext(pScene);
	}

	void RenderGui();
private:
	ResourceManager* pResourceManager;

	HierarchyPanel HierarchyPanel;
	InspectorPanel InspectorPanel;

	Camera Camera;
};