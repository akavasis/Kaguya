#pragma once
#include <thread>
#include <filesystem>
#include "RenderSystem.h"
#include "Window.h"
#include "Time.h"

//----------------------------------------------------------------------------------------------------
class Application
{
public:
	inline static std::filesystem::path			ExecutableFolderPath;
	inline static Window*						pWindow					= nullptr;
	inline static RenderSystem*					pRenderSystem			= nullptr;
	inline static std::thread*					RenderThread			= nullptr;
	inline static std::atomic<bool>				ExitRenderThread		= false;

	static void Initialize(LPCWSTR WindowName,
		int Width = CW_USEDEFAULT, int Height = CW_USEDEFAULT,
		int X = CW_USEDEFAULT, int Y = CW_USEDEFAULT);

	static int Run(RenderSystem* pRenderSystem);
private:
	static void RenderThreadMain();

	static bool HandleRenderMessage(const Window::Message& Message);
};