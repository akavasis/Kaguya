#pragma once
#include <thread>
#include <filesystem>
#include "Window.h"
#include "RenderSystem.h"

//----------------------------------------------------------------------------------------------------
class Application
{
public:
	static void Initialize(LPCWSTR WindowName,
		int32_t Width = CW_USEDEFAULT, int32_t Height = CW_USEDEFAULT,
		int32_t X = CW_USEDEFAULT, int32_t Y = CW_USEDEFAULT);

	static int Run(RenderSystem* pRenderSystem);

	inline static std::filesystem::path			ExecutableFolderPath;
	inline static Window*						pWindow					= nullptr;
	inline static RenderSystem*					pRenderSystem			= nullptr;
private:
	static void RenderThreadMain();
	static bool HandleRenderMessage(const Window::Message& Message);

	inline static std::thread					RenderThread;
	inline static bool							ExitRenderThread		= false;
};