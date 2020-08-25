#pragma once
#define WIN32_LEAN_AND_MEAN
#include <Windows.h>
#include <filesystem>
#include "Delegate.h"

class Application
{
public:
	Application();
	~Application();

	std::filesystem::path ExecutableFolderPath() const;
	int Run(Delegate<void()> Callback);
private:
	std::filesystem::path m_ExecutableFolderPath;
	Delegate<void()> m_Callback;
};