#pragma once
#include "UIWindow.h"

class ViewportWindow : public UIWindow
{
public:
	ViewportWindow()
		: UIWindow(false)
	{

	}

	std::pair<float, float> GetMousePosition() const;

	void RenderGui(void* pImage);
public:
	Vector2i Resolution;
	RECT Rect;
};