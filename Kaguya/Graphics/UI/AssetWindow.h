#pragma once
#include "UIWindow.h"

class AssetWindow : public UIWindow
{
public:
	AssetWindow()
		: UIWindow(true)
	{

	}

	void RenderGui();
};