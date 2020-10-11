#include "pch.h"
#include "Application.h"
#include "Time.h"

#include <windowsx.h>
#include <shellapi.h>
#include <shellscalingapi.h>
#pragma comment(lib, "shcore.lib")

void Application::Initialize(LPCWSTR WindowName,
	int32_t Width /*= CW_USEDEFAULT*/, int32_t Height /*= CW_USEDEFAULT*/,
	int32_t X /*= CW_USEDEFAULT*/, int32_t Y /*= CW_USEDEFAULT*/)
{
	Log::Create();

	ThrowCOMIfFailed(CoInitializeEx(NULL, tagCOINIT::COINIT_APARTMENTTHREADED));
	SetProcessDpiAwareness(PROCESS_DPI_AWARENESS::PROCESS_SYSTEM_DPI_AWARE);

	int argc;
	LPWSTR* argv = CommandLineToArgvW(GetCommandLineW(), &argc);
	if (argv)
	{
		std::filesystem::path executablePath{ argv[0] };
		ExecutableFolderPath = executablePath.parent_path();
		LocalFree(argv);
	}

	pWindow = new Window(WindowName, Width, Height, X, Y);
}

int Application::Run(RenderSystem* pRenderSystem)
{
	int ExitCode = EXIT_SUCCESS;

	try
	{
		if (!pRenderSystem)
			throw;

		// Begin our render thread
		Application::pRenderSystem = pRenderSystem;
		ExitRenderThread = false;
		RenderThread = new std::thread(RenderThreadMain);

		MSG msg = {};
		while (msg.message != WM_QUIT)
		{
			if (PeekMessage(&msg, NULL, 0, 0, PM_REMOVE))
			{
				TranslateMessage(&msg);
				DispatchMessage(&msg);
			}
		}

		ExitCode = (int)msg.wParam;
	}
	catch (std::exception& e)
	{
		ExitCode = EXIT_FAILURE;
	}
	catch (...)
	{
		ExitCode = EXIT_FAILURE;
	}

	if (RenderThread)
	{
		ExitRenderThread = true;

		// Join the thread
		RenderThread->join();
		delete RenderThread;
		RenderThread = nullptr;
	}

	if (Application::pRenderSystem)
	{
		delete Application::pRenderSystem;
		Application::pRenderSystem = nullptr;
	}

	if (pWindow)
	{
		delete pWindow;
		pWindow = nullptr;
	}

	CoUninitialize();
	return ExitCode;
}

void Application::RenderThreadMain()
{
	// Set to high priority thread
	SetThreadDescription(GetCurrentThread(), L"Render Thread");
	SetThreadPriority(GetCurrentThread(), THREAD_PRIORITY_HIGHEST);

	Time Time;
	Time.Restart();

	pRenderSystem->OnInitialize();
	while (!ExitRenderThread)
	{
		// Process window messages
		{
			Window::Message messsage;
			Window::Message resizeMessage(Window::Message::Type::None, {});
			while (pWindow->MessageQueue.pop(messsage, 0))
			{
				if (messsage.type == Window::Message::Type::Resize)
				{
					resizeMessage = messsage;
					continue;
				}

				HandleRenderMessage(messsage);
			}

			// Now we process resize message
			if (resizeMessage.type == Window::Message::Type::Resize)
			{
				if (!HandleRenderMessage(resizeMessage))
				{
					// Requeue this message if it failed to resize
					pWindow->MessageQueue.push(resizeMessage);
				}
			}
		}

		// Process mouse & keyboard events
		{
			// Handle input
			while (!pWindow->Keyboard.KeyBufferIsEmpty())
			{
				auto e = pWindow->Keyboard.ReadKey();
				if (e.type != Keyboard::Event::Type::Press)
					continue;
				switch (e.data.Code)
				{
				case VK_ESCAPE:
				{
					if (pWindow->CursorEnabled())
					{
						pWindow->DisableCursor();
						pWindow->Mouse.EnableRawInput();
					}
					else
					{
						pWindow->EnableCursor();
						pWindow->Mouse.DisableRawInput();
					}
				}
				break;
				}
			}

			// Handle render system mouse events
			while (!pWindow->Mouse.RawDeltaBufferIsEmpty())
			{
				auto delta = pWindow->Mouse.ReadRawDelta();

				if (!pWindow->CursorEnabled())
				{
					pRenderSystem->OnHandleMouse(delta.X, delta.Y, Time.DeltaTime());
				}
			}

			// Handle render system keyboard events
			if (!pWindow->CursorEnabled())
			{
				pRenderSystem->OnHandleKeyboard(pWindow->Keyboard, Time.DeltaTime());
			}
		}

		Time.Signal();
		pRenderSystem->OnUpdate(Time);
		pRenderSystem->OnRender();
	}
	pRenderSystem->OnDestroy();
}

bool Application::HandleRenderMessage(const Window::Message& Message)
{
	switch (Message.type)
	{
	case Window::Message::Type::Resize:
	{
		pRenderSystem->UpdateForResize(Message.data.Width, Message.data.Height);
		pRenderSystem->OnResize(Message.data.Width, Message.data.Height);

		return true;
	}
	break;
	}

	return false;
}