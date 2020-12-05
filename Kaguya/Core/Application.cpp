#include "pch.h"
#include "Application.h"
#include "Time.h"

#include <windowsx.h>
#include <shellapi.h>
#include <shellscalingapi.h>
#pragma comment(lib, "shcore.lib")

//----------------------------------------------------------------------------------------------------
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

//----------------------------------------------------------------------------------------------------
int Application::Run(RenderSystem* pRenderSystem)
{
	int ExitCode = EXIT_SUCCESS;

	try
	{
		if (!pRenderSystem)
			throw std::exception("Null RenderSystem");

		// Begin our render thread
		ExitRenderThread			= false;
		RenderThread				= wil::unique_handle(::CreateThread(NULL, 0, Application::RenderThreadProc, pRenderSystem, 0, nullptr));
		if (!RenderThread)
		{
			LOG_ERROR("Failed to create thread (error={})", ::GetLastError());
		}

		MSG msg = {};
		while (msg.message != WM_QUIT)
		{
			if (PeekMessage(&msg, NULL, 0, 0, PM_REMOVE))
			{
				TranslateMessage(&msg);
				DispatchMessage(&msg);
			}
			else
			{
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
			}
		}

		ExitCode = (int)msg.wParam;
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

	if (pWindow)
	{
		delete pWindow;
		pWindow = nullptr;
	}

	CoUninitialize();
	return ExitCode;
}

//----------------------------------------------------------------------------------------------------
DWORD WINAPI Application::RenderThreadProc(_In_ PVOID pParameter)
{
	SetThreadDescription(GetCurrentThread(), L"Render Thread");

	ThrowCOMIfFailed(CoInitializeEx(NULL, tagCOINIT::COINIT_APARTMENTTHREADED));
	auto pRenderSystem = reinterpret_cast<RenderSystem*>(pParameter);
	
	Time Time;
	Time.Restart();

	if (!pRenderSystem->OnInitialize())
		goto Error;

	while (!ExitRenderThread)
	{
		if (!pWindow->IsFocused())
			continue;

		Time.Signal();

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

				HandleRenderMessage(pRenderSystem, messsage);
			}

			// Now we process resize message
			if (resizeMessage.type == Window::Message::Type::Resize)
			{
				if (!HandleRenderMessage(pRenderSystem, resizeMessage))
				{
					// Requeue this message if it failed to resize
					pWindow->MessageQueue.push(resizeMessage);
				}
			}
		}

		// Update
		pRenderSystem->OnUpdate(Time);

		// Process mouse & keyboard events
		{
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
		
		pRenderSystem->OnRender();
	}

Error:
	pRenderSystem->OnDestroy();
	CoUninitialize();

	return EXIT_SUCCESS;
}

//----------------------------------------------------------------------------------------------------
bool Application::HandleRenderMessage(RenderSystem* pRenderSystem, const Window::Message& Message)
{
	switch (Message.type)
	{
	case Window::Message::Type::Resize:
	{
		return pRenderSystem->OnResize(Message.data.Width, Message.data.Height);
	}
	break;
	}

	return false;
}