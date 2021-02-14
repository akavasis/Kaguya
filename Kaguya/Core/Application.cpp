#include "pch.h"
#include "Application.h"

#include <windowsx.h>
#include <shellapi.h>
#include <shellscalingapi.h>
#pragma comment(lib, "shcore.lib")

void Application::Initialize(const Config& Config)
{
	Log::Create();

	ThrowIfFailed(CoInitializeEx(nullptr, tagCOINIT::COINIT_APARTMENTTHREADED));
	SetProcessDpiAwareness(PROCESS_DPI_AWARENESS::PROCESS_SYSTEM_DPI_AWARE);

	int argc;
	LPWSTR* argv = CommandLineToArgvW(GetCommandLineW(), &argc);
	if (argv)
	{
		std::filesystem::path executablePath{ argv[0] };
		ExecutableFolderPath = executablePath.parent_path();
		LocalFree(argv);
	}

	auto ImageFile = Application::ExecutableFolderPath / "Assets/Kaguya.ico";
	Window.SetIcon(::LoadImage(0, ImageFile.wstring().data(), IMAGE_ICON, 0, 0, LR_DEFAULTSIZE | LR_LOADFROMFILE));
	Window.Create(Config.Title.data(), Config.Width, Config.Height, Config.X, Config.Y, Config.Maximize);
	InputHandler.Create(&Window);

	MinimumWidth = GetSystemMetrics(SM_CXMINTRACK);
	MinimumHeight = GetSystemMetrics(SM_CYMINTRACK);
}

int Application::Run(std::function<void()> ShutdownFunc)
{
	int ExitCode = EXIT_SUCCESS;

	try
	{
		// Begin our render thread
		RenderThread = wil::unique_handle(::CreateThread(nullptr, 0, Application::RenderThreadProc, nullptr, 0, nullptr));
		if (!RenderThread)
		{
			LOG_ERROR("Failed to create thread (error={})", ::GetLastError());
			Quit();
		}

		MSG Msg = {};
		while (Msg.message != WM_QUIT)
		{
			if (::PeekMessage(&Msg, nullptr, 0, 0, PM_REMOVE))
			{
				::TranslateMessage(&Msg);
				::DispatchMessage(&Msg);

				InputHandler.Handle(&Msg);
			}

			if (QuitApplication)
			{
				break;
			}
		}

		ExitCode = (int)Msg.wParam;
	}
	catch (std::exception& e)
	{
		ExitCode = EXIT_FAILURE;
		MessageBoxA(nullptr, e.what(), "Error", MB_OK | MB_ICONERROR | MB_DEFAULT_DESKTOP_ONLY);
	}
	catch (...)
	{
		ExitCode = EXIT_FAILURE;
		MessageBoxA(nullptr, nullptr, "Unknown Error", MB_OK | MB_ICONERROR | MB_DEFAULT_DESKTOP_ONLY);
	}

	// Set ExitRenderThread to true and wait for it to join
	if (RenderThread)
	{
		ExitRenderThread = true;
		::WaitForSingleObject(RenderThread.get(), INFINITE);
	}

	if (ShutdownFunc)
	{
		ShutdownFunc();
	}

	CoUninitialize();
	LOG_INFO("Exit Code: {}", ExitCode);
	return ExitCode;
}

void Application::Quit()
{
	QuitApplication = true;
}

DWORD WINAPI Application::RenderThreadProc(_In_ PVOID pParameter)
{
	SetThreadDescription(GetCurrentThread(), L"Render Thread");

	ThrowIfFailed(CoInitializeEx(nullptr, tagCOINIT::COINIT_APARTMENTTHREADED));

	while (!ExitRenderThread)
	{
		// Process window messages
		Window::Message messsage;
		Window::Message resizeMessage(Window::Message::EType::None, {});
		while (Window.MessageQueue.Dequeue(messsage, 0))
		{
			if (messsage.Type == Window::Message::EType::Resize)
			{
				resizeMessage = messsage;
				continue;
			}

			HandleRenderMessage(messsage);
		}

		// Now we process resize message
		if (resizeMessage.Type == Window::Message::EType::Resize)
		{
			if (!HandleRenderMessage(resizeMessage))
			{
				// Requeue this message if it failed to resize
				Window.MessageQueue.Enqueue(resizeMessage);
			}
		}

		Application::Window.Render();
	}

	CoUninitialize();

	Application::Quit();

	return EXIT_SUCCESS;
}

bool Application::HandleRenderMessage(const Window::Message& Message)
{
	switch (Message.Type)
	{
	case Window::Message::EType::Resize:
	{
		Application::Window.Resize(Message.Data.Width, Message.Data.Height);
		return true;
	}
	break;
	}

	return false;
}