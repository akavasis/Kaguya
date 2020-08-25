#include "pch.h"
#include "Application.h"
#include <shellapi.h>

Application::Application()
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

std::filesystem::path Application::ExecutableFolderPath() const
{
	return m_ExecutableFolderPath;
}

int Application::Run(Delegate<void()> Callback)
{
	m_Callback = Callback;
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
			m_Callback();
		}
	}
	return static_cast<int>(msg.wParam);
}