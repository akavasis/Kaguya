#pragma once
#include <wil/resource.h>
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

	static void Quit();

private:
	static DWORD WINAPI RenderThreadProc(_In_ PVOID pParameter);
	static bool HandleRenderMessage(RenderSystem* pRenderSystem, const Window::Message& Message);

public:
	inline static std::filesystem::path			ExecutableFolderPath;
	inline static Window						Window;

private:
	inline static std::atomic<bool>				QuitApplication			= false;
	inline static wil::unique_handle			RenderThread;
	inline static std::atomic<bool>				ExitRenderThread		= false;
};