#pragma once
#include <thread>
#include <filesystem>
#include "Window.h"
#include "RenderSystem.h"

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
		int32_t Width = CW_USEDEFAULT, int32_t Height = CW_USEDEFAULT,
		int32_t X = CW_USEDEFAULT, int32_t Y = CW_USEDEFAULT);

	static int Run(RenderSystem* pRenderSystem);
private:
	static void RenderThreadMain();

	static bool HandleRenderMessage(const Window::Message& Message);
};