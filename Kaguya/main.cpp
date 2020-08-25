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

#include "Graphics/Scene/MaterialLoader.h"
#include "Graphics/Scene/ModelLoader.h"
#include "Graphics/Scene/Scene.h"

int main(int argc, char** argv)
{
	try
	{
#if defined(_DEBUG)
		ENABLE_LEAK_DETECTION();
		SET_LEAK_BREAKPOINT(-1);
#endif

		Application application;
		Window window{ L"DXR" };
		Renderer renderer{ application, window };

		MaterialLoader materialLoader{ application.ExecutableFolderPath() };
		ModelLoader modelLoader{ application.ExecutableFolderPath() };

		Scene scene;
		scene.Skybox.Path = application.ExecutableFolderPath() / "Assets/IBL/ChiricahuaPath.hdr";

		scene.Camera.SetLens(DirectX::XM_PIDIV4, 1.0f, 0.1f, 500.0f);
		scene.Camera.SetPosition(0.0f, 5.0f, -10.0f);

		auto& nullMaterial = scene.AddMaterial(materialLoader.LoadMaterial(
			nullptr,
			nullptr,
			nullptr,
			nullptr,
			nullptr
		));
		nullMaterial.Properties.Albedo = { 1.0f, 0.0f, 0.0f };
		nullMaterial.Properties.Roughness = 0.0f;
		nullMaterial.Properties.Metallic = 1.0f;

		auto& nullMaterial1 = scene.AddMaterial(materialLoader.LoadMaterial(
			nullptr,
			nullptr,
			nullptr,
			nullptr,
			nullptr
		));
		nullMaterial1.Properties.Albedo = { 1.0f, 1.0f, 1.0f };
		nullMaterial1.Properties.Roughness = 0.0f;
		nullMaterial1.Properties.Metallic = 0.0f;

		auto& cerberusMat = scene.AddMaterial(materialLoader.LoadMaterial(
			"Assets/Models/Cerberus/Textures/Cerberus_Albedo.dds",
			"Assets/Models/Cerberus/Textures/Cerberus_Normal.dds",
			"Assets/Models/Cerberus/Textures/Cerberus_Roughness.dds",
			"Assets/Models/Cerberus/Textures/Cerberus_Metallic.dds",
			nullptr
		));

		auto& cerberus = scene.AddModel(modelLoader.LoadFromFile("Assets/Models/Cerberus/Cerberus_LP.fbx", 0.05f));
		cerberus.Meshes[0].MaterialIndex = 2;
		cerberus.Translate(0.0f, 5.0f, -5.0f);
		cerberus.Rotate(DirectX::XMConvertToRadians(90.0f), DirectX::XMConvertToRadians(90.0f), 0.0f);

		auto& sphere = scene.AddModel(modelLoader.LoadFromFile("Assets/Models/Sphere/Sphere.obj"));

		auto& grid = scene.AddModel(CreateGrid(100.0f, 100.0f, 10, 10));
		grid.Translate(0.0f, -10.0f, 0.0f);
		grid.Meshes[0].MaterialIndex = 1;

		renderer.UploadScene(scene);

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
			std::wstring msg = L"   FPS: " + std::to_wstring(Renderer::Statistics::FPS);
			msg += L"   FPMS: " + std::to_wstring(Renderer::Statistics::FPMS);
			window.AppendToTitle(msg);
			renderer.Update(time);
			renderer.Render(scene);
		});
	}
	catch (std::exception& e)
	{
		MessageBoxA(nullptr, e.what(), "ERROR: !KAI LAUNCHED NUCLEAR BOMBS!", MB_OK | MB_ICONERROR | MB_DEFAULT_DESKTOP_ONLY);
		return 0;
	}
	catch (...)
	{
		MessageBoxA(nullptr, "ERROR: UNKNOWN", "ERROR: !KAI LAUNCHED NUCLEAR BOMBS!", MB_OK | MB_ICONERROR | MB_DEFAULT_DESKTOP_ONLY);
		return 0;
	}
}