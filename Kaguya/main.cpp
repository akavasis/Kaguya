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

using namespace DirectX;

int main(int argc, char** argv)
{
	try
	{
#if defined(_DEBUG)
		ENABLE_LEAK_DETECTION();
		SET_LEAK_BREAKPOINT(-1);
#endif

		Application application;
		Window window{ L"Path Tracer" };
		Renderer renderer{ application, window };

		MaterialLoader materialLoader{ application.ExecutableFolderPath() };
		ModelLoader modelLoader{ application.ExecutableFolderPath() };

		Scene scene;
		scene.Skybox.Path = application.ExecutableFolderPath() / "Assets/IBL/ChiricahuaPath.hdr";

		scene.Camera.SetLens(DirectX::XM_PIDIV4, 1.0f, 0.1f, 500.0f);
		scene.Camera.SetPosition(0.0f, 5.0f, -20.0f);

		// Materials
		{
			auto& nullMaterial = scene.AddMaterial(materialLoader.LoadMaterial(0, 0, 0, 0, 0));
			nullMaterial.Properties.Albedo = { 0.7f, 0.7f, 0.7f };

			auto& leftwallMaterial = scene.AddMaterial(materialLoader.LoadMaterial(0, 0, 0, 0, 0));
			leftwallMaterial.Properties.Albedo = { 0.7f, 0.1f, 0.1f };

			auto& rightwallMaterial = scene.AddMaterial(materialLoader.LoadMaterial(0, 0, 0, 0, 0));
			rightwallMaterial.Properties.Albedo = { 0.1f, 0.7f, 0.1f };

			auto& lightMaterial = scene.AddMaterial(materialLoader.LoadMaterial(0, 0, 0, 0, 0));
			lightMaterial.Properties.Albedo = { 0.0f, 0.0f, 0.0f };
			XMStoreFloat3(&lightMaterial.Properties.Emissive, XMVectorSet(1.0f, 0.9f, 0.7f, 0.0f) * 20.0f);

			auto& leftsphereMaterial = scene.AddMaterial(materialLoader.LoadMaterial(0, 0, 0, 0, 0));
			leftsphereMaterial.Properties.Albedo = { 0.9f, 0.9f, 0.5f };
			leftsphereMaterial.Properties.PercentSpecular = 0.1f;
			leftsphereMaterial.Properties.Roughness = 0.2f;
			leftsphereMaterial.Properties.Specular = { 0.9f, 0.9f, 0.9f };

			auto& middlesphereMaterial = scene.AddMaterial(materialLoader.LoadMaterial(0, 0, 0, 0, 0));
			middlesphereMaterial.Properties.Albedo = { 0.9f, 0.5f, 0.9f };
			middlesphereMaterial.Properties.PercentSpecular = 0.3f;
			middlesphereMaterial.Properties.Roughness = 0.2f;
			middlesphereMaterial.Properties.Specular = { 0.9f, 0.9f, 0.9f };

			// a ball which has blue diffuse but red specular. an example of a "bad material".
			// a better lighting model wouldn't let you do this sort of thing
			auto& rightsphereMaterial = scene.AddMaterial(materialLoader.LoadMaterial(0, 0, 0, 0, 0));
			rightsphereMaterial.Properties.Albedo = { 0.0f, 0.0f, 1.0f };
			rightsphereMaterial.Properties.PercentSpecular = 0.5f;
			rightsphereMaterial.Properties.Roughness = 0.4f;
			rightsphereMaterial.Properties.Specular = { 1.0f, 0.0f, 0.0f };

			float variousRoughness[5] = { 0.0f, 0.25f, 0.5f, 0.75f, 1.0f };
			for (int i = 0; i < 5; ++i)
			{
				auto& greenMat = scene.AddMaterial(materialLoader.LoadMaterial(0, 0, 0, 0, 0));
				greenMat.Properties.Albedo = { 1.0f, 1.0f, 1.0f };
				greenMat.Properties.Emissive = { 0.0f, 0.0f, 0.0f };
				greenMat.Properties.PercentSpecular = 1.0f;
				greenMat.Properties.Roughness = variousRoughness[i];
				greenMat.Properties.Specular = { 0.3f, 1.0f, 0.3f };
			}
		}

		// Walls
		{
			auto& floor = scene.AddModel(CreateGrid(10.0f, 10.0f, 10, 10));
			floor.Meshes[0].MaterialIndex = 0;

			auto& ceiling = scene.AddModel(CreateGrid(10.0f, 10.0f, 10, 10));
			ceiling.Translate(0.0f, 10.0f, 0.0f);
			ceiling.Meshes[0].MaterialIndex = 0;

			auto& backwall = scene.AddModel(CreateGrid(10.0f, 10.0f, 10, 10));
			backwall.Translate(0.0f, 5.0f, 5.0f);
			backwall.Rotate(DirectX::XM_PIDIV2, 0.0f, 0.0f);
			backwall.Meshes[0].MaterialIndex = 0;

			auto& leftwall = scene.AddModel(CreateGrid(10.0f, 10.0f, 10, 10));
			leftwall.Translate(-5.0f, 5.0f, 0.0f);
			leftwall.Rotate(0.0f, 0.0f, DirectX::XM_PIDIV2);
			leftwall.Meshes[0].MaterialIndex = 1;

			auto& rightwall = scene.AddModel(CreateGrid(10.0f, 10.0f, 10, 10));
			rightwall.Translate(+5.0f, 5.0f, 0.0f);
			rightwall.Rotate(0.0f, 0.0f, DirectX::XM_PIDIV2);
			rightwall.Meshes[0].MaterialIndex = 2;
		}

		// Light
		{
			auto& light = scene.AddModel(CreateGrid(5.0f, 5.0f, 10, 10));
			light.Translate(0.0f, 9.9f, 1.0f);
			light.Rotate(0.0f, 0.0f, XM_PI);
			light.Meshes[0].MaterialIndex = 3;
		}

		// Spheres
		{
			auto& left = scene.AddModel(modelLoader.LoadFromFile("Assets/Models/Sphere/Sphere.obj"));
			left.Translate(-4.0f, 1.0f, 0.0f);
			left.Meshes[0].MaterialIndex = 4;

			auto& middle = scene.AddModel(modelLoader.LoadFromFile("Assets/Models/Sphere/Sphere.obj"));
			middle.Translate(0.0f, 1.0f, 0.0f);
			middle.Meshes[0].MaterialIndex = 5;

			auto& right = scene.AddModel(modelLoader.LoadFromFile("Assets/Models/Sphere/Sphere.obj"));
			right.Translate(4.0f, 1.0f, 0.0f);
			right.Meshes[0].MaterialIndex = 6;

			// Shiny green spheres of varying roughnesses
			for (int i = 0; i < 5; ++i)
			{
				float dx = i * 2.0f - 4.0f;

				auto& greenSphere = scene.AddModel(modelLoader.LoadFromFile("Assets/Models/Sphere/Sphere.obj", 0.75f));
				greenSphere.Translate(dx, 5.0f, 4.0f);
				greenSphere.Meshes[0].MaterialIndex = i + 7;
			}
		}

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