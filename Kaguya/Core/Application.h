#pragma once
#define WIN32_LEAN_AND_MEAN
#include <Windows.h>
#include <functional>
#include <filesystem>

class Application
{
public:
	using Callback = std::function<void()>;

	Application();
	~Application();

	std::filesystem::path ExecutableFolderPath();
	int Run(Callback CallBack);
private:
	std::filesystem::path m_ExecutableFolderPath;
};