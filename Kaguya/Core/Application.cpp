#include "pch.h"
#include "Application.h"

#include <windowsx.h>
#include <shellapi.h>
#include <shellscalingapi.h>
#pragma comment(lib, "shcore.lib")

#include "Time.h"

//----------------------------------------------------------------------------------------------------
void Application::Initialize(LPCWSTR WindowName,
	int32_t Width /*= CW_USEDEFAULT*/, int32_t Height /*= CW_USEDEFAULT*/,
	int32_t X /*= CW_USEDEFAULT*/, int32_t Y /*= CW_USEDEFAULT*/)
{
	Log::Create();

	ThrowIfFailed(CoInitializeEx(NULL, tagCOINIT::COINIT_APARTMENTTHREADED));
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
	Window.Create(WindowName, Width, Height, X, Y);
	InputHandler.Create(&Window);

	MinimumWidth = GetSystemMetrics(SM_CXMINTRACK);
	MinimumHeight = GetSystemMetrics(SM_CYMINTRACK);
}

//----------------------------------------------------------------------------------------------------
int Application::Run(RenderSystem* pRenderSystem)
{
	int ExitCode = EXIT_SUCCESS;
	std::unique_ptr<RenderSystem> RenderSystem(pRenderSystem);

	try
	{
		if (!pRenderSystem)
			throw std::exception("Null RenderSystem");

		// Begin our render thread
		RenderThread = wil::unique_handle(::CreateThread(NULL, 0, Application::RenderThreadProc, RenderSystem.get(), 0, nullptr));
		if (!RenderThread)
		{
			LOG_ERROR("Failed to create thread (error={})", ::GetLastError());
			Quit();
		}

		MSG Msg = {};
		while (Msg.message != WM_QUIT)
		{
			if (::PeekMessage(&Msg, NULL, 0, 0, PM_REMOVE))
			{
				::TranslateMessage(&Msg);
				::DispatchMessage(&Msg);

				InputHandler.Handle(&Msg);

				if (InputHandler.Mouse.IsRMBPressed())
				{
					Window.DisableCursor();
					InputHandler.Mouse.UseRawInput = true;
				}
				else
				{
					Window.EnableCursor();
					InputHandler.Mouse.UseRawInput = false;
				}
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

	CoUninitialize();
	LOG_INFO("Exit Code: {}", ExitCode);
	return ExitCode;
}

void Application::Quit()
{
	QuitApplication = true;
}

//----------------------------------------------------------------------------------------------------
DWORD WINAPI Application::RenderThreadProc(_In_ PVOID pParameter)
{
	SetThreadDescription(GetCurrentThread(), L"Render Thread");

	ThrowIfFailed(CoInitializeEx(NULL, tagCOINIT::COINIT_APARTMENTTHREADED));
	auto pRenderSystem = reinterpret_cast<RenderSystem*>(pParameter);
	
	Time Time;
	Time.Restart();

	if (!pRenderSystem->OnInitialize())
		goto Error;

	while (!ExitRenderThread)
	{
		if (!Window.IsFocused())
			continue;

		Time.Signal();

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

			HandleRenderMessage(pRenderSystem, messsage);
		}

		// Now we process resize message
		if (resizeMessage.Type == Window::Message::EType::Resize)
		{
			if (!HandleRenderMessage(pRenderSystem, resizeMessage))
			{
				// Requeue this message if it failed to resize
				Window.MessageQueue.Enqueue(resizeMessage);
			}
		}

		// Update
		pRenderSystem->OnUpdate(Time);

		pRenderSystem->OnHandleInput(Time.DeltaTime());

		if (!Window.CursorEnabled())
		{
			pRenderSystem->OnHandleRawInput(Time.DeltaTime());
		}

		pRenderSystem->OnRender();
	}

Error:
	pRenderSystem->OnDestroy();
	CoUninitialize();

	Application::Quit();

	return EXIT_SUCCESS;
}

//----------------------------------------------------------------------------------------------------
bool Application::HandleRenderMessage(RenderSystem* pRenderSystem, const Window::Message& Message)
{
	switch (Message.Type)
	{
	case Window::Message::EType::Resize:
	{
		return pRenderSystem->OnResize(Message.Data.Width, Message.Data.Height);
	}
	break;
	}

	return false;
}