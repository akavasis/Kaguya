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
#include <sstream>
#include <exception>

#include "Core/Application.h"
#include "Core/Window.h"
#include "Core/Time.h"

#include "Graphics/Renderer.h"
#include "Graphics/Scene/Scene.h"
#include "Graphics/Scene/Camera.h"

int main(int argc, char** argv)
{
	ENABLE_LEAK_DETECTION();
	SET_LEAK_BREAKPOINT(-1);

	try
	{
		Application application;
		Window window{ L"DirectX12 Window" };
		Renderer renderer{ window };

		PerspectiveCamera camera;
		camera.SetLens(DirectX::XM_PIDIV4, 1.0f, 0.1f, 500.0f);
		camera.SetPosition(0.0f, 5.0f, -10.0f);

		// Load Scene
		Scene scene;

#if 0
		auto helmet = std::make_unique<Model>("../../Assets/Models/DamagedHelmet/glTF-Embedded/DamagedHelmet.gltf", 2.0f);
		helmet->Translate(0.0f, 0.0f, 10.0f);
		helmet->Rotate(DirectX::XMConvertToRadians(-90.0f), 0.0f, 0.0f);
		scene.AddModel(std::move(helmet));
#endif

#if 1
		auto cerberus = std::make_unique<Model>("../../Assets/Models/Cerberus/Cerberus_LP.fbx", 0.05f);
		cerberus->GetMaterials()[0]->Maps[TextureType::Albedo] = "../../Assets/Models/Cerberus/Textures/Cerberus_A.tga";
		cerberus->GetMaterials()[0]->Maps[TextureType::Normal] = "../../Assets/Models/Cerberus/Textures/Cerberus_N.tga";
		cerberus->GetMaterials()[0]->Maps[TextureType::Roughness] = "../../Assets/Models/Cerberus/Textures/Cerberus_R.tga";
		cerberus->GetMaterials()[0]->Maps[TextureType::Metallic] = "../../Assets/Models/Cerberus/Textures/Cerberus_M.tga";

		cerberus->GetMaterials()[0]->Flags |= Material::Flags::MATERIAL_FLAG_HAVE_ALBEDO_TEXTURE;
		cerberus->GetMaterials()[0]->Flags |= Material::Flags::MATERIAL_FLAG_HAVE_NORMAL_TEXTURE;
		cerberus->GetMaterials()[0]->Flags |= Material::Flags::MATERIAL_FLAG_HAVE_ROUGHNESS_TEXTURE;
		cerberus->GetMaterials()[0]->Flags |= Material::Flags::MATERIAL_FLAG_HAVE_METALLIC_TEXTURE;
		cerberus->Translate(0.0f, 5.0f, -5.0f);
		cerberus->Rotate(DirectX::XMConvertToRadians(90.0f), DirectX::XMConvertToRadians(90.0f), 0.0f);
		scene.AddModel(std::move(cerberus));

		Material mat;
		mat.Properties.Albedo = { 1.0f, 1.0f, 1.0f };

		int sphereCount = 5;
		for (int y = 0; y < sphereCount; ++y)
		{
			for (int x = 0; x < sphereCount; ++x)
			{
				mat.Properties.Roughness = float(x) / (float(sphereCount) - 1.0f);
				mat.Properties.Metallic = float(y) / (float(sphereCount) - 1.0f);
				auto sphere = Model::CreateSphereFromFile(mat);
				float dx = x * 2.0f - 2.0f;
				float dy = y * 2.0f - 2.0f;
				sphere->Translate(dx, dy, 0.0f);
				scene.AddModel(std::move(sphere));
			}
		}

		mat.Properties.Albedo = { 1.0f,1.0f, 1.0f };
		mat.Properties.Roughness = 1.0f;
		mat.Properties.Metallic = 0.5f;
		auto grid = Model::CreateGrid(100.0f, 100.0f, 10, 10, mat);
		grid->Translate(0.0f, -10.0f, 0.0f);
		scene.AddModel(std::move(grid));
#endif

		scene.AddPointLight(PointLight());

		// Set renderer's active objects
		renderer.SetActiveCamera(&camera);
		renderer.SetActiveScene(&scene);

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
					camera.Translate(0.0f, 0.0f, time.DeltaTime());
				if (window.GetKeyboard().IsKeyPressed('A'))
					camera.Translate(-time.DeltaTime(), 0.0f, 0.0f);
				if (window.GetKeyboard().IsKeyPressed('S'))
					camera.Translate(0.0f, 0.0f, -time.DeltaTime());
				if (window.GetKeyboard().IsKeyPressed('D'))
					camera.Translate(time.DeltaTime(), 0.0f, 0.0f);
				if (window.GetKeyboard().IsKeyPressed('Q'))
					camera.Translate(0.0f, time.DeltaTime(), 0.0f);
				if (window.GetKeyboard().IsKeyPressed('E'))
					camera.Translate(0.0f, -time.DeltaTime(), 0.0f);
			}

			while (!window.GetMouse().RawDeltaBufferIsEmpty())
			{
				const auto delta = window.GetMouse().ReadRawDelta();
				if (!window.CursorEnabled())
				{
					camera.Rotate(delta.Y * time.DeltaTime(), delta.X * time.DeltaTime());
				}
			}

			time.Signal();
			renderer.Update(time);
			renderer.Render();
			renderer.Present();
		});
	}
	catch (std::exception& e)
	{
		MessageBoxA(nullptr, e.what(), "ERROR: !KAI LAUNCHED NUCLEAR BOMBS!", MB_OK | MB_ICONERROR | MB_DEFAULT_DESKTOP_ONLY);
		return 0;
	}
}