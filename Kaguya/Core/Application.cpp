#include "pch.h"
#include "Application.h"
#include <shellapi.h>

Application::Application(LPCWSTR WindowName,
	int Width, int Height,
	int X, int Y)
	: Window(WindowName, Width, Height, X, Y)
{
	Log::Create();
	ThrowCOMIfFailed(CoInitializeEx(NULL, tagCOINIT::COINIT_APARTMENTTHREADED));
	SetThreadDpiAwarenessContext(DPI_AWARENESS_CONTEXT_PER_MONITOR_AWARE_V2);

	int argc;
	LPWSTR* argv = CommandLineToArgvW(GetCommandLineW(), &argc);
	if (argv)
	{
		std::filesystem::path executablePath{ argv[0] };
		m_ExecutableFolderPath = executablePath.parent_path();
		CORE_INFO("Executable folder path: {}", m_ExecutableFolderPath.generic_string());
		LocalFree(argv);
	}
	else
	{
		CORE_ERROR("Call to CommandLineToArgvW failed");
	}
}

Application::~Application()
{
	CoUninitialize();
}