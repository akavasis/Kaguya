#pragma once

class UIWindow
{
public:
	UIWindow(bool IsTab)
		: IsTab(IsTab)
	{

	}

	void Update();

	bool IsHovered;
protected:
	bool IsTab;
};