// main.cpp : Defines the entry point for the application.
//
#if _DEBUG
// memory leak
#define _CRTDBG_MAP_ALLOC
#include <cstdlib>
#include <crtdbg.h>
#define ENABLE_LEAK_DETECTION() _CrtSetDbgFlag(_CRTDBG_ALLOC_MEM_DF | _CRTDBG_LEAK_CHECK_DF)
#define SET_LEAK_BREAKPOINT(x) _CrtSetBreakAlloc(x)
#else
#define ENABLE_LEAK_DETECTION() 0
#define SET_LEAK_BREAKPOINT(X) X
#endif
#include <exception>

#include "Core/Application.h"
#include "Core/Window.h"
#include "Core/Time.h"
#include "Graphics/Renderer.h"
#include "Graphics/ModelAllocator.h"

int main(int argc, char** argv)
{
	try
	{
#if defined(_DEBUG)
		ENABLE_LEAK_DETECTION();
		SET_LEAK_BREAKPOINT(-1);
#endif

		Application application;
		Window window{ L"Kaguya Shinomiya" };
		Renderer renderer{ application, window };

		ModelLoader modelLoader{ application.ExecutableFolderPath() };
		modelLoader.LoadFromFile("Assets/Models/Cerberus/Cerberus_LP.fbx");
		modelLoader.LoadFromFile("Assets/Models/Sphere/Sphere.obj");

		Scene scene;
		scene.Camera.SetLens(DirectX::XM_PIDIV4, 1.0f, 0.1f, 500.0f);
		scene.Camera.SetPosition(0.0f, 5.0f, -10.0f);

		Time time;
		time.Restart();
		return application.Run([&]()
		{
			// Handle input
			while (!window.GetKeyboard().KeyBufferIsEmpty())
			{
				const Keyboard::Event e = window.GetKeyboard().ReadKey();
				if (e.type != Keyboard::Event::Type::Press)
					continue;
				switch (e.data.Code)
				{
				case VK_ESCAPE:
				{
					if (window.CursorEnabled())
					{
						window.DisableCursor();
						window.GetMouse().EnableRawInput();
					}
					else
					{
						window.EnableCursor();
						window.GetMouse().DisableRawInput();
					}
				}
				break;
				}
			}

			if (!window.CursorEnabled())
			{
				if (window.GetKeyboard().IsKeyPressed('W'))
					scene.Camera.Translate(0.0f, 0.0f, time.DeltaTime());
				if (window.GetKeyboard().IsKeyPressed('A'))
					scene.Camera.Translate(-time.DeltaTime(), 0.0f, 0.0f);
				if (window.GetKeyboard().IsKeyPressed('S'))
					scene.Camera.Translate(0.0f, 0.0f, -time.DeltaTime());
				if (window.GetKeyboard().IsKeyPressed('D'))
					scene.Camera.Translate(time.DeltaTime(), 0.0f, 0.0f);
				if (window.GetKeyboard().IsKeyPressed('Q'))
					scene.Camera.Translate(0.0f, time.DeltaTime(), 0.0f);
				if (window.GetKeyboard().IsKeyPressed('E'))
					scene.Camera.Translate(0.0f, -time.DeltaTime(), 0.0f);
			}

			while (!window.GetMouse().RawDeltaBufferIsEmpty())
			{
				const auto delta = window.GetMouse().ReadRawDelta();
				if (!window.CursorEnabled())
				{
					scene.Camera.Rotate(delta.Y * time.DeltaTime(), delta.X * time.DeltaTime());
				}
			}

			time.Signal();
			renderer.Update(time);
			//renderer.Render();
			//renderer.Present();
		});
	}
	catch (std::exception& e)
	{
		MessageBoxA(nullptr, e.what(), "ERROR: !KAI LAUNCHED NUCLEAR BOMBS!", MB_OK | MB_ICONERROR | MB_DEFAULT_DESKTOP_ONLY);
		return 0;
	}
}