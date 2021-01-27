#pragma once

#include <wil/resource.h>
#include <filesystem>

#include "Window.h"
#include "InputHandler.h"
#include "RenderSystem.h"

//----------------------------------------------------------------------------------------------------
class Application
{
public:
	struct Config
	{
		std::wstring Title	= L"Application";
		int Width			= CW_USEDEFAULT;
		int Height			= CW_USEDEFAULT;
		int X				= CW_USEDEFAULT;
		int Y				= CW_USEDEFAULT;
		bool Maximize		= false;
	};

	static void Initialize(const Config& Config);

	static int Run(RenderSystem* pRenderSystem);

	static void Quit();
private:
	static DWORD WINAPI RenderThreadProc(_In_ PVOID pParameter);
	static bool HandleRenderMessage(RenderSystem* pRenderSystem, const Window::Message& Message);
public:
	inline static std::filesystem::path			ExecutableFolderPath;
	inline static Window						Window;
	inline static InputHandler					InputHandler;

	inline static int							MinimumWidth			= 0;
	inline static int							MinimumHeight			= 0;
private:
	inline static std::atomic<bool>				QuitApplication			= false;
	inline static wil::unique_handle			RenderThread;
	inline static std::atomic<bool>				ExitRenderThread		= false;
};