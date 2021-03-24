#pragma once
#include "UIWindow.h"

#include "../Scene/Scene.h"
#include "../Scene/Entity.h"

class AssetWindow : public UIWindow
{
public:
	AssetWindow()
		: UIWindow(true)
	{

	}

	void RenderGui();

	Scene* pScene = nullptr;;
};