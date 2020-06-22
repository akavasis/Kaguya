#include "pch.h"
#include "Application.h"
#include <shellapi.h>

Application::Application()
{
	ThrowCOMIfFailed(CoInitializeEx(NULL, tagCOINIT::COINIT_APARTMENTTHREADED));
	SetThreadDpiAwarenessContext(DPI_AWARENESS_CONTEXT_PER_MONITOR_AWARE_V2);

	int argc;
	LPWSTR* argv = CommandLineToArgvW(GetCommandLineW(), &argc);
	if (argv)
	{
		std::filesystem::path executablePath{ argv[0] };
		m_ExecutableFolderPath = executablePath.parent_path();

		LocalFree(argv);
	}
	else
	{
		CORE_ERROR("Call to CommandLineToArgvW failed");
	}

	Log::Create();
	CORE_INFO("Log::Create invoked");
}

Application::~Application()
{
	CoUninitialize();
}

std::filesystem::path Application::ExecutableFolderPath()
{
	return m_ExecutableFolderPath;
}

int Application::Run(Callback CallBack)
{
	// Main message loop:
	MSG msg = {};
	while (msg.message != WM_QUIT)
	{
		// Process all messages, stop on WM_QUIT
		if (PeekMessage(&msg, NULL, 0, 0, PM_REMOVE))
		{
			// Translates messages and sends them to WndProc
			// WindowProcedure internally calls this->DispatchEvent
			TranslateMessage(&msg);
			DispatchMessage(&msg);
		}
		else
		{
			CallBack();
		}
	}
	return static_cast<int>(msg.wParam);
}