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
#include "Graphics/Scene/Scene.h"

using namespace DirectX;

Scene RandomScene(const MaterialLoader& MaterialLoader, const ModelLoader& ModelLoader)
{
	Scene scene;

	// Materials
	{
		// 0
		auto& nullMaterial = scene.AddMaterial(MaterialLoader.LoadMaterial(0, 0, 0, 0, 0));
		nullMaterial.Albedo = { 0.7f, 0.7f, 0.7f };
		nullMaterial.Model = LambertianModel;
	}

	int matIdx = 1;
	// Models
	{
		auto& floor = scene.AddModel(CreateGrid(500.0f, 500.0f, 10, 10));
		floor.Meshes[0].MaterialIndex = 0;

		for (int a = -11; a < 11; a++)
		{
			for (int b = -11; b < 11; b++)
			{
				auto choose_mat = Math::RandF();
				XMFLOAT3 center0(a + 0.9f * Math::RandF(), 0.2f, b + 0.9f * Math::RandF());
				XMVECTOR centerVector = XMLoadFloat3(&center0);
				if (XMVectorGetX(XMVector3Length(centerVector - XMVectorSet(4.0f, 0.2f, 0.0f, 1.0f))) > 0.9f)
				{
					auto& mat = scene.AddMaterial(MaterialLoader.LoadMaterial(0, 0, 0, 0, 0));
					auto& sphere = scene.AddModel(ModelLoader.LoadFromFile("Assets/Models/Sphere/Sphere.obj", 0.2f));
					sphere.Translate(center0.x, center0.y, center0.z);
					sphere.Meshes[0].MaterialIndex = matIdx++;

					if (choose_mat < 0.8f)
					{
						XMVECTOR albedo = Math::RandomVector() * Math::RandomVector();
						XMStoreFloat3(&mat.Albedo, albedo);
						mat.Model = LambertianModel;
					}
					else if (choose_mat < 0.95f)
					{
						XMVECTOR albedo = Math::RandomVector(0.5, 1);
						float fuzz = Math::RandF(0, 0.5);

						XMStoreFloat3(&mat.Albedo, albedo);
						mat.Fuzziness = fuzz;
						mat.Model = MetalModel;
					}
					else
					{
						mat.IndexOfRefraction = 1.5f;
						mat.Model = DielectricModel;
					}
				}
			}
		}
	}

	// Sphere 1
	{
		auto& material1 = scene.AddMaterial(MaterialLoader.LoadMaterial(0, 0, 0, 0, 0));
		material1.IndexOfRefraction = 1.5f;
		material1.Model = DielectricModel;

		auto& sphere1 = scene.AddModel(ModelLoader.LoadFromFile("Assets/Models/Sphere/Sphere.obj"));
		sphere1.Translate(0.0f, 1.0f, 0.0f);
		sphere1.Meshes[0].MaterialIndex = matIdx++;
	}

	// Sphere 2
	{
		auto& material2 = scene.AddMaterial(MaterialLoader.LoadMaterial(0, 0, 0, 0, 0));
		material2.Albedo = { 0.4f, 0.2f, 0.1f };
		material2.Model = LambertianModel;

		auto& sphere2 = scene.AddModel(ModelLoader.LoadFromFile("Assets/Models/Sphere/Sphere.obj"));
		sphere2.Translate(-4.0f, 1.0f, 0.0f);
		sphere2.Meshes[0].MaterialIndex = matIdx++;
	}

	// Sphere 3
	{
		auto& material3 = scene.AddMaterial(MaterialLoader.LoadMaterial(0, 0, 0, 0, 0));
		material3.Albedo = { 0.7f, 0.6f, 0.5f };
		material3.Fuzziness = 0.0f;
		material3.Model = MetalModel;

		auto& sphere3 = scene.AddModel(ModelLoader.LoadFromFile("Assets/Models/Sphere/Sphere.obj"));
		sphere3.Translate(4.0f, 1.0f, 0.0f);
		sphere3.Meshes[0].MaterialIndex = matIdx++;
	}

	return scene;
}

Scene CornellBox(const MaterialLoader& MaterialLoader, const ModelLoader& ModelLoader)
{
	Scene scene;

	// Materials
	{
		// 0
		auto& nullMaterial = scene.AddMaterial(MaterialLoader.LoadMaterial(0, 0, 0, 0, 0));
		nullMaterial.Albedo = { 0.7f, 0.7f, 0.7f };
		nullMaterial.Model = LambertianModel;

		// 1
		auto& leftwallMaterial = scene.AddMaterial(MaterialLoader.LoadMaterial(0, 0, 0, 0, 0));
		leftwallMaterial.Albedo = { 0.7f, 0.1f, 0.1f };
		leftwallMaterial.Model = LambertianModel;

		// 2
		auto& rightwallMaterial = scene.AddMaterial(MaterialLoader.LoadMaterial(0, 0, 0, 0, 0));
		rightwallMaterial.Albedo = { 0.1f, 0.7f, 0.1f };
		rightwallMaterial.Model = LambertianModel;

		// 3
		auto& lightMaterial = scene.AddMaterial(MaterialLoader.LoadMaterial(0, 0, 0, 0, 0));
		lightMaterial.Albedo = { 0.0f, 0.0f, 0.0f };
		XMStoreFloat3(&lightMaterial.Emissive, XMVectorSet(1.0f, 0.9f, 0.7f, 0.0f) * 20.0f);
		lightMaterial.Model = DiffuseLightModel;
	}

	// Models
	{
		auto& floor = scene.AddModel(CreateGrid(10.0f, 10.0f, 10, 10));
		floor.Meshes[0].MaterialIndex = 0;

		auto& ceiling = scene.AddModel(CreateGrid(10.0f, 10.0f, 10, 10));
		ceiling.Translate(0.0f, 10.0f, 0.0f);
		ceiling.Rotate(0.0f, 0.0f, XM_PI);
		ceiling.Meshes[0].MaterialIndex = 0;

		auto& backwall = scene.AddModel(CreateGrid(10.0f, 10.0f, 10, 10));
		backwall.Translate(0.0f, 5.0f, 5.0f);
		backwall.Rotate(-DirectX::XM_PIDIV2, 0.0f, 0.0f);
		backwall.Meshes[0].MaterialIndex = 0;

		auto& leftwall = scene.AddModel(CreateGrid(10.0f, 10.0f, 10, 10));
		leftwall.Translate(-5.0f, 5.0f, 0.0f);
		leftwall.Rotate(0.0f, 0.0f, -DirectX::XM_PIDIV2);
		leftwall.Meshes[0].MaterialIndex = 1;

		auto& rightwall = scene.AddModel(CreateGrid(10.0f, 10.0f, 10, 10));
		rightwall.Translate(+5.0f, 5.0f, 0.0f);
		rightwall.Rotate(0.0f, 0.0f, DirectX::XM_PIDIV2);
		rightwall.Meshes[0].MaterialIndex = 2;

		auto& light = scene.AddModel(CreateGrid(5.0f, 5.0f, 10, 10));
		light.Translate(0.0f, 9.9f, 1.0f);
		light.Meshes[0].MaterialIndex = 3;
	}

	return scene;
}

Scene LambertianSpheresInCornellBox(const MaterialLoader& MaterialLoader, const ModelLoader& ModelLoader)
{
	Scene scene = CornellBox(MaterialLoader, ModelLoader);

	// Materials
	{
		// 4
		auto& leftsphereMaterial = scene.AddMaterial(MaterialLoader.LoadMaterial(0, 0, 0, 0, 0));
		leftsphereMaterial.Albedo = { 0.9f, 0.9f, 0.5f };
		leftsphereMaterial.Model = LambertianModel;

		// 5
		auto& middlesphereMaterial = scene.AddMaterial(MaterialLoader.LoadMaterial(0, 0, 0, 0, 0));
		middlesphereMaterial.Albedo = { 0.9f, 0.5f, 0.9f };
		middlesphereMaterial.Model = LambertianModel;

		// 6
		auto& rightsphereMaterial = scene.AddMaterial(MaterialLoader.LoadMaterial(0, 0, 0, 0, 0));
		rightsphereMaterial.Albedo = { 0.5f, 0.9f, 0.9f };
		rightsphereMaterial.Model = LambertianModel;
	}

	// Spheres
	{
		auto& left = scene.AddModel(ModelLoader.LoadFromFile("Assets/Models/Sphere/Sphere.obj"));
		left.Translate(-4.0f, 1.0f, 0.0f);
		left.Meshes[0].MaterialIndex = 4;

		auto& middle = scene.AddModel(ModelLoader.LoadFromFile("Assets/Models/Sphere/Sphere.obj"));
		middle.Translate(0.0f, 1.0f, 0.0f);
		middle.Meshes[0].MaterialIndex = 5;

		auto& right = scene.AddModel(ModelLoader.LoadFromFile("Assets/Models/Sphere/Sphere.obj"));
		right.Translate(4.0f, 1.0f, 0.0f);
		right.Meshes[0].MaterialIndex = 6;
	}

	return scene;
}

Scene GlossySpheresInCornellBox(const MaterialLoader& MaterialLoader, const ModelLoader& ModelLoader)
{
	Scene scene = CornellBox(MaterialLoader, ModelLoader);

	// Materials
	{
		// 4
		auto& leftsphereMaterial = scene.AddMaterial(MaterialLoader.LoadMaterial(0, 0, 0, 0, 0));
		leftsphereMaterial.Albedo = { 0.9f, 0.9f, 0.5f };
		leftsphereMaterial.SpecularChance = 0.1f;
		leftsphereMaterial.SpecularRoughness = 0.2f;
		leftsphereMaterial.SpecularColor = { 0.9f, 0.9f, 0.9f };
		leftsphereMaterial.Model = GlossyModel;

		// 5
		auto& middlesphereMaterial = scene.AddMaterial(MaterialLoader.LoadMaterial(0, 0, 0, 0, 0));
		middlesphereMaterial.Albedo = { 0.9f, 0.5f, 0.9f };
		middlesphereMaterial.SpecularChance = 0.3f;
		middlesphereMaterial.SpecularRoughness = 0.2f;
		middlesphereMaterial.SpecularColor = { 0.9f, 0.9f, 0.9f };
		middlesphereMaterial.Model = GlossyModel;

		// 6
		// a ball which has blue diffuse but red specular. an example of a "bad material".
		// a better lighting model wouldn't let you do this sort of thing
		auto& rightsphereMaterial = scene.AddMaterial(MaterialLoader.LoadMaterial(0, 0, 0, 0, 0));
		rightsphereMaterial.Albedo = { 0.0f, 0.0f, 1.0f };
		rightsphereMaterial.SpecularChance = 0.5f;
		rightsphereMaterial.SpecularRoughness = 0.4f;
		rightsphereMaterial.SpecularColor = { 1.0f, 0.0f, 0.0f };
		rightsphereMaterial.Model = GlossyModel;

		// 7 + i
		float variousRoughness[5] = { 0.0f, 0.25f, 0.5f, 0.75f, 1.0f };
		for (int i = 0; i < 5; ++i)
		{
			auto& greenMat = scene.AddMaterial(MaterialLoader.LoadMaterial(0, 0, 0, 0, 0));
			greenMat.Albedo = { 1.0f, 1.0f, 1.0f };
			greenMat.SpecularChance = 1.0f;
			greenMat.SpecularRoughness = variousRoughness[i];
			greenMat.SpecularColor = { 0.3f, 1.0f, 0.3f };
			greenMat.Model = GlossyModel;
		}
	}

	// Spheres
	{
		auto& left = scene.AddModel(ModelLoader.LoadFromFile("Assets/Models/Sphere/Sphere.obj", 1.25f));
		left.Translate(-3.25f, 1.25f, 0.0f);
		left.Meshes[0].MaterialIndex = 4;

		auto& middle = scene.AddModel(ModelLoader.LoadFromFile("Assets/Models/Sphere/Sphere.obj", 1.25f));
		middle.Translate(0.0f, 1.25f, 0.0f);
		middle.Meshes[0].MaterialIndex = 5;

		auto& right = scene.AddModel(ModelLoader.LoadFromFile("Assets/Models/Sphere/Sphere.obj", 1.25f));
		right.Translate(3.25f, 1.25f, 0.0f);
		right.Meshes[0].MaterialIndex = 6;

		// Shiny green spheres of varying roughnesses
		for (int i = 0; i < 5; ++i)
		{
			float dx = i * 2.0f - 4.0f;

			auto& greenSphere = scene.AddModel(ModelLoader.LoadFromFile("Assets/Models/Sphere/Sphere.obj", 0.75f));
			greenSphere.Translate(dx, 5.0f, 4.0f);
			greenSphere.Meshes[0].MaterialIndex = i + 7;
		}
	}

	return scene;
}

Scene TransparentSpheresOfIncreasingIoR(const MaterialLoader& MaterialLoader, const ModelLoader& ModelLoader)
{
	Scene scene;
	const int c_numSpheres = 7;

	// Materials
	{
		auto& defaultMat = scene.AddMaterial(MaterialLoader.LoadMaterial(0, 0, 0, 0, 0));
		defaultMat.Albedo = { 0.7f, 0.7f, 0.7f };
		defaultMat.Model = LambertianModel;

		auto& light = scene.AddMaterial(MaterialLoader.LoadMaterial(0, 0, 0, 0, 0));
		XMStoreFloat3(&light.Emissive, XMVectorSet(1.0f, 0.9f, 0.7f, 0.0f) * 20.0f);
		light.Model = LambertianModel;

		for (int sphereIndex = 0; sphereIndex < c_numSpheres; ++sphereIndex)
		{
			float indexOfRefraction = 1.0f + 0.5f * float(sphereIndex) / float(c_numSpheres - 1);

			auto& sphere = scene.AddMaterial(MaterialLoader.LoadMaterial(0, 0, 0, 0, 0));

			sphere.Albedo = { 0.9f, 0.25f, 0.25f };
			sphere.Emissive = { 0.0f, 0.0f, 0.0f };
			sphere.SpecularRoughness = 0.0f;
			sphere.SpecularColor = { 0.8f, 0.8f, 0.8f };
			sphere.IndexOfRefraction = indexOfRefraction;
			sphere.RefractionRoughness = 0.0f;
			sphere.Model = DielectricModel;
		}
	}

	// Models
	{
		auto& floor = scene.AddModel(CreateGrid(30.0f, 10.0f, 10, 10));
		floor.Meshes[0].MaterialIndex = 0;

		auto& ceiling = scene.AddModel(CreateGrid(10.0f, 10.0f, 10, 10));
		ceiling.Translate(0.0f, 10.0f, 0.0f);
		ceiling.Rotate(0.0f, 0.0f, XM_PI);
		ceiling.Meshes[0].MaterialIndex = 0;

		auto& light = scene.AddModel(CreateGrid(5.0f, 5.0f, 10, 10));
		light.Translate(0.0f, 9.9f, 1.0f);
		light.Meshes[0].MaterialIndex = 1;

		for (int i = 0; i < c_numSpheres; ++i)
		{
			float dx = i * 4.0f - 12.0f;

			auto& sphere = scene.AddModel(ModelLoader.LoadFromFile("Assets/Models/Sphere/Sphere.obj"));
			sphere.Translate(dx, 2.0f, 0.0f);
			sphere.Meshes[0].MaterialIndex = i + 2;
		}
	}

	return scene;
}

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
		Scene scene = TransparentSpheresOfIncreasingIoR(materialLoader, modelLoader);
		scene.Skybox.Path = application.ExecutableFolderPath() / "Assets/IBL/ChiricahuaPath.hdr";

		scene.Camera.SetLens(DirectX::XM_PIDIV4, 1.0f, 0.1f, 500.0f);
		scene.Camera.SetPosition(0.0f, 5.0f, -20.0f);

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