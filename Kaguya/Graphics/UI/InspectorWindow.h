#pragma once
#include "UIWindow.h"

#include "../Scene/Scene.h"
#include "../Scene/Entity.h"

class InspectorWindow : public UIWindow
{
public:
	InspectorWindow()
		: UIWindow(false)
	{

	}

	void SetContext(Scene* pScene, Entity Entity)
	{
		this->pScene = pScene;
		SelectedEntity = Entity;
	}

	void RenderGui(float x, float y, float width, float height);
private:
	Scene* pScene;
	Entity SelectedEntity = {};
};