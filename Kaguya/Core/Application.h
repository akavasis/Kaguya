#pragma once
#include <filesystem>
#include "Window.h"
#include "Time.h"

class Application
{
public:
	Application(LPCWSTR WindowName,
		int Width = CW_USEDEFAULT, int Height = CW_USEDEFAULT,
		int X = CW_USEDEFAULT, int Y = CW_USEDEFAULT);
	virtual ~Application();

	virtual int Run() = 0;

	inline auto GetExecutableFolderPath() const { return m_ExecutableFolderPath; }

	Window Window;
	Time Time;
private:
	std::filesystem::path m_ExecutableFolderPath;
};