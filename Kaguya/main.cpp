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
#include "Graphics/Renderer.h"
#include "Graphics/Scene/Scene.h"

using namespace DirectX;

Scene RandomScene(const MaterialLoader& MaterialLoader, const ModelLoader& ModelLoader)
{
	Scene scene;

	// Materials
	auto& defaultMaterial = scene.AddMaterial(MaterialLoader.LoadMaterial(0, 0, 0, 0, 0));
	defaultMaterial.Albedo = { 0.7f, 0.7f, 0.7f };
	defaultMaterial.Model = LambertianModel;

	// Models
	auto& floor = scene.AddModel(CreateGrid(500.0f, 500.0f, 10, 10));
	auto& sphere = scene.AddModel(ModelLoader.LoadFromFile("Assets/Models/Sphere/Sphere.obj"));

	// Model instances
	auto& floorInstance = scene.AddModelInstance({ &floor, &defaultMaterial });

	for (int a = -11; a < 11; a++)
	{
		for (int b = -11; b < 11; b++)
		{
			auto choose_mat = Math::RandF();
			XMFLOAT3 center0(a + 0.9f * Math::RandF(), 0.2f, b + 0.9f * Math::RandF());
			XMVECTOR centerVector = XMLoadFloat3(&center0);
			if (XMVectorGetX(XMVector3Length(centerVector - XMVectorSet(4.0f, 0.2f, 0.0f, 1.0f))) > 0.9f)
			{
				auto& material = scene.AddMaterial(MaterialLoader.LoadMaterial(0, 0, 0, 0, 0));
				auto& sphereInstance = scene.AddModelInstance({ &sphere, &material });
				sphereInstance.SetScale(0.2f);
				sphereInstance.Translate(center0.x, center0.y, center0.z);

				if (choose_mat < 0.25f && choose_mat >= 0.0f)
				{
					XMVECTOR albedo = Math::RandomVector() * Math::RandomVector();

					XMStoreFloat3(&material.Albedo, albedo);
					material.Model = LambertianModel;
				}
				else if (choose_mat < 0.5f && choose_mat >= 0.25f)
				{
					XMVECTOR albedo = Math::RandomVector();
					XMVECTOR specularColor = Math::RandomVector();

					XMStoreFloat3(&material.Albedo, albedo);
					material.SpecularChance = Math::RandF();
					material.Roughness = Math::RandF();
					XMStoreFloat3(&material.Specular, specularColor);
					material.Model = GlossyModel;
				}
				else if (choose_mat < 0.75f && choose_mat >= 0.5f)
				{
					material.IndexOfRefraction = 1.5f;
					material.Model = DielectricModel;
				}
				else
				{
					XMVECTOR albedo = Math::RandomVector();
					float fuzz = Math::RandF();

					XMStoreFloat3(&material.Albedo, albedo);
					material.Fuzziness = fuzz;
					material.Model = MetalModel;
				}
			}
		}
	}

	// Sphere 1
	{
		auto& material = scene.AddMaterial(MaterialLoader.LoadMaterial(0, 0, 0, 0, 0));
		material.Albedo = { 0.4f, 0.2f, 0.1f };
		material.Model = LambertianModel;

		auto& sphereInstance = scene.AddModelInstance({ &sphere, &material });
		sphereInstance.Translate(-3.0f, 1.0f, 0.0f);
	}

	// Sphere 2
	{
		auto& material = scene.AddMaterial(MaterialLoader.LoadMaterial(0, 0, 0, 0, 0));
		material.Albedo = { 0.9f, 0.9f, 0.5f };
		material.SpecularChance = 0.3f;
		material.Roughness = 0.2f;
		material.Specular = { 0.9f, 0.9f, 0.9f };
		material.Model = GlossyModel;

		auto& sphereInstance = scene.AddModelInstance({ &sphere, &material });
		sphereInstance.Translate(-1.0f, 1.0f, 0.0f);
	}

	// Sphere 3
	{
		auto& material = scene.AddMaterial(MaterialLoader.LoadMaterial(0, 0, 0, 0, 0));
		material.Albedo = { 0.7f, 0.6f, 0.5f };
		material.Fuzziness = 0.0f;
		material.Model = MetalModel;

		auto& sphereInstance = scene.AddModelInstance({ &sphere, &material });
		sphereInstance.Translate(1.0f, 1.0f, 0.0f);
	}

	// Sphere 4
	{
		auto& material = scene.AddMaterial(MaterialLoader.LoadMaterial(0, 0, 0, 0, 0));
		material.IndexOfRefraction = 1.5f;
		material.Model = DielectricModel;

		auto& sphereInstance = scene.AddModelInstance({ &sphere, &material });
		sphereInstance.Translate(3.0f, 1.0f, 0.0f);
	}

	return scene;
}

void AddCornellBox(const MaterialLoader& MaterialLoader, const ModelLoader& ModelLoader, Scene* pScene)
{
	assert(pScene != nullptr);
	// Materials
	auto& defaultMat = pScene->AddMaterial(MaterialLoader.LoadMaterial(0, 0, 0, 0, 0));
	defaultMat.Albedo = { 0.7f, 0.7f, 0.7f };
	defaultMat.Model = LambertianModel;

	auto& leftWallMat = pScene->AddMaterial(MaterialLoader.LoadMaterial(0, 0, 0, 0, 0));
	leftWallMat.Albedo = { 0.7f, 0.1f, 0.1f };
	leftWallMat.Model = LambertianModel;

	auto& rightWallMat = pScene->AddMaterial(MaterialLoader.LoadMaterial(0, 0, 0, 0, 0));
	rightWallMat.Albedo = { 0.1f, 0.7f, 0.1f };
	rightWallMat.Model = LambertianModel;

	auto& lightMat = pScene->AddMaterial(MaterialLoader.LoadMaterial(0, 0, 0, 0, 0));
	lightMat.Albedo = { 0.0f, 0.0f, 0.0f };
	XMStoreFloat3(&lightMat.Emissive, XMVectorSet(1.0f, 0.9f, 0.7f, 0.0f) * 20.0f);
	lightMat.Model = DiffuseLightModel;

	// Models
	auto& rect = pScene->AddModel(CreateGrid(10.0f, 10.0f, 10, 10));
	auto& light = pScene->AddModel(CreateGrid(5.0f, 5.0f, 10, 10));

	// Model instances
	auto& floorInstance = pScene->AddModelInstance({ &rect, &defaultMat });
	auto& ceilingInstance = pScene->AddModelInstance({ &rect, &defaultMat });
	ceilingInstance.Translate(0.0f, 10.0f, 0.0f);
	ceilingInstance.Rotate(0.0f, 0.0f, XM_PI);
	auto& backwallInstance = pScene->AddModelInstance({ &rect, &defaultMat });
	backwallInstance.Translate(0.0f, 5.0f, 5.0f);
	backwallInstance.Rotate(-DirectX::XM_PIDIV2, 0.0f, 0.0f);
	auto& leftwallInstance = pScene->AddModelInstance({ &rect, &leftWallMat });
	leftwallInstance.Translate(-5.0f, 5.0f, 0.0f);
	leftwallInstance.Rotate(0.0f, 0.0f, -DirectX::XM_PIDIV2);
	auto& rightwallInstance = pScene->AddModelInstance({ &rect, &rightWallMat });
	rightwallInstance.Translate(+5.0f, 5.0f, 0.0f);
	rightwallInstance.Rotate(0.0f, 0.0f, DirectX::XM_PIDIV2);
	auto& lightInstance = pScene->AddModelInstance({ &light, &lightMat });
	lightInstance.Translate(0.0f, 9.9f, 0.0f);
}

Scene LambertianSpheresInCornellBox(const MaterialLoader& MaterialLoader, const ModelLoader& ModelLoader)
{
	Scene scene; AddCornellBox(MaterialLoader, ModelLoader, &scene);

	// Materials
	auto& leftSphereMaterial = scene.AddMaterial(MaterialLoader.LoadMaterial(0, 0, 0, 0, 0));
	leftSphereMaterial.Albedo = { 0.9f, 0.9f, 0.5f };
	leftSphereMaterial.Model = LambertianModel;

	auto& middleSphereMaterial = scene.AddMaterial(MaterialLoader.LoadMaterial(0, 0, 0, 0, 0));
	middleSphereMaterial.Albedo = { 0.9f, 0.5f, 0.9f };
	middleSphereMaterial.Model = LambertianModel;

	auto& rightSphereMaterial = scene.AddMaterial(MaterialLoader.LoadMaterial(0, 0, 0, 0, 0));
	rightSphereMaterial.Albedo = { 0.5f, 0.9f, 0.9f };
	rightSphereMaterial.Model = LambertianModel;

	// Models
	auto& sphere = scene.AddModel(ModelLoader.LoadFromFile("Assets/Models/Sphere/Sphere.obj"));

	// Model instances
	auto& leftSphereInstance = scene.AddModelInstance({ &sphere, &leftSphereMaterial });
	leftSphereInstance.SetScale(1.25f);
	leftSphereInstance.Translate(-3.25f, 1.25f, 0.0f);

	auto& middleSphereInstance = scene.AddModelInstance({ &sphere, &middleSphereMaterial });
	middleSphereInstance.SetScale(1.25f);
	middleSphereInstance.Translate(0.0f, 1.25f, 0.0f);

	auto& rightSphereInstance = scene.AddModelInstance({ &sphere, &rightSphereMaterial });
	rightSphereInstance.SetScale(1.25f);
	rightSphereInstance.Translate(3.25f, 1.25f, 0.0f);

	return scene;
}

Scene GlossySpheresInCornellBox(const MaterialLoader& MaterialLoader, const ModelLoader& ModelLoader)
{
	Scene scene; AddCornellBox(MaterialLoader, ModelLoader, &scene);
	constexpr int NumSpheres = 5;

	// Materials
	auto& leftSphereMaterial = scene.AddMaterial(MaterialLoader.LoadMaterial(0, 0, 0, 0, 0));
	leftSphereMaterial.Albedo = { 0.9f, 0.9f, 0.5f };
	leftSphereMaterial.SpecularChance = 0.1f;
	leftSphereMaterial.Roughness = 0.2f;
	leftSphereMaterial.Specular = { 0.9f, 0.9f, 0.9f };
	leftSphereMaterial.Model = GlossyModel;

	auto& middleSphereMaterial = scene.AddMaterial(MaterialLoader.LoadMaterial(0, 0, 0, 0, 0));
	middleSphereMaterial.Albedo = { 0.9f, 0.5f, 0.9f };
	middleSphereMaterial.SpecularChance = 0.3f;
	middleSphereMaterial.Roughness = 0.2f;
	middleSphereMaterial.Specular = { 0.9f, 0.9f, 0.9f };
	middleSphereMaterial.Model = GlossyModel;

	auto& rightSphereMaterial = scene.AddMaterial(MaterialLoader.LoadMaterial(0, 0, 0, 0, 0));
	rightSphereMaterial.Albedo = { 0.0f, 0.0f, 1.0f };
	rightSphereMaterial.SpecularChance = 0.5f;
	rightSphereMaterial.Roughness = 0.4f;
	rightSphereMaterial.Specular = { 1.0f, 0.0f, 0.0f };
	rightSphereMaterial.Model = GlossyModel;

	Material* sphereMats[NumSpheres];
	for (int i = 0; i < NumSpheres; ++i)
	{
		float roughness = float(i) * 0.25f;

		sphereMats[i] = &scene.AddMaterial(MaterialLoader.LoadMaterial(0, 0, 0, 0, 0));
		sphereMats[i]->Albedo = { 1.0f, 1.0f, 1.0f };
		sphereMats[i]->SpecularChance = 1.0f;
		sphereMats[i]->Roughness = roughness;
		sphereMats[i]->Specular = { 0.3f, 1.0f, 0.3f };
		sphereMats[i]->Model = GlossyModel;
	}

	// Models
	auto& sphere = scene.AddModel(ModelLoader.LoadFromFile("Assets/Models/Sphere/Sphere.obj"));

	// Model instances
	auto& leftSphereInstance = scene.AddModelInstance({ &sphere, &leftSphereMaterial });
	leftSphereInstance.SetScale(1.25f);
	leftSphereInstance.Translate(-3.25f, 1.25f, 0.0f);

	auto& middleSphereInstance = scene.AddModelInstance({ &sphere, &middleSphereMaterial });
	middleSphereInstance.SetScale(1.25f);
	middleSphereInstance.Translate(0.0f, 1.25f, 0.0f);

	auto& rightSphereInstance = scene.AddModelInstance({ &sphere, &rightSphereMaterial });
	rightSphereInstance.SetScale(1.25f);
	rightSphereInstance.Translate(3.25f, 1.25f, 0.0f);

	// Shiny green spheres of varying roughnesses
	for (int i = 0; i < NumSpheres; ++i)
	{
		float dx = i * 2.0f - 4.0f;

		auto& sphereInstance = scene.AddModelInstance({ &sphere, sphereMats[i] });
		sphereInstance.SetScale(0.75f);
		sphereInstance.Translate(dx, 5.0f, 4.0f);
	}

	return scene;
}

Scene TransparentSpheresOfIncreasingIoR(const MaterialLoader& MaterialLoader, const ModelLoader& ModelLoader)
{
	Scene scene;
	constexpr int NumSpheres = 7;

	// Materials
	auto& defaultMat = scene.AddMaterial(MaterialLoader.LoadMaterial(0, 0, 0, 0, 0));
	defaultMat.Albedo = { 0.7f, 0.7f, 0.7f };
	defaultMat.Model = LambertianModel;

	auto& lightMat = scene.AddMaterial(MaterialLoader.LoadMaterial(0, 0, 0, 0, 0));
	XMStoreFloat3(&lightMat.Emissive, XMVectorSet(1.0f, 0.9f, 0.7f, 0.0f) * 20.0f);
	lightMat.Model = LambertianModel;

	Material* sphereMats[NumSpheres];
	for (int i = 0; i < NumSpheres; ++i)
	{
		float indexOfRefraction = 1.0f + 0.5f * float(i) / float(NumSpheres - 1);

		sphereMats[i] = &scene.AddMaterial(MaterialLoader.LoadMaterial(0, 0, 0, 0, 0));

		sphereMats[i]->Albedo = { 0.9f, 0.25f, 0.25f };
		sphereMats[i]->Emissive = { 0.0f, 0.0f, 0.0f };
		sphereMats[i]->Roughness = 0.0f;
		sphereMats[i]->Specular = { 0.8f, 0.8f, 0.8f };
		sphereMats[i]->IndexOfRefraction = indexOfRefraction;
		sphereMats[i]->Model = DielectricModel;
	}

	// Models
	auto& floor = scene.AddModel(CreateGrid(30.0f, 10.0f, 10, 10));
	auto& ceiling = scene.AddModel(CreateGrid(10.0f, 10.0f, 10, 10));
	auto& light = scene.AddModel(CreateGrid(5.0f, 5.0f, 10, 10));
	auto& sphere = scene.AddModel(ModelLoader.LoadFromFile("Assets/Models/Sphere/Sphere.obj"));

	// Model instances
	auto& floorInstance = scene.AddModelInstance({ &floor, &defaultMat });
	auto& ceilingInstance = scene.AddModelInstance({ &ceiling, &defaultMat });
	ceilingInstance.Translate(0.0f, 10.0f, 0.0f);
	ceilingInstance.Rotate(0.0f, 0.0f, XM_PI);

	auto& lightInstance = scene.AddModelInstance({ &light, &lightMat });
	lightInstance.Translate(0.0f, 9.9f, 0.0f);

	for (int i = 0; i < NumSpheres; ++i)
	{
		float dx = i * 4.0f - 12.0f;

		auto& sphereInstance = scene.AddModelInstance({ &sphere, sphereMats[i] });
		sphereInstance.Translate(dx, 2.0f, 0.0f);
	}

	return scene;
}

int main(int argc, char** argv)
{
#if defined(_DEBUG)
	ENABLE_LEAK_DETECTION();
	SET_LEAK_BREAKPOINT(-1);
#endif

	Renderer* pRenderer = nullptr;
	try
	{
		Application::Initialize(L"Path Tracer", 1280, 720);
		{
			pRenderer = new Renderer(*Application::pWindow);

			MaterialLoader materialLoader(Application::ExecutableFolderPath);
			ModelLoader modelLoader(Application::ExecutableFolderPath);

			Scene scene = GlossySpheresInCornellBox(materialLoader, modelLoader);
			scene.Skybox.Path = Application::ExecutableFolderPath / "Assets/IBL/ChiricahuaPath.hdr";

			scene.Camera.SetLens(DirectX::XM_PIDIV4, 1.0f, 0.1f, 500.0f);
			scene.Camera.SetPosition(0.0f, 5.0f, -20.0f);

			pRenderer->SetScene(&scene);
			return Application::Run(pRenderer);
		}
	}
	catch (std::exception& e)
	{
		MessageBoxA(nullptr, e.what(), "Error", MB_OK | MB_ICONERROR | MB_DEFAULT_DESKTOP_ONLY);
	}
	catch (...)
	{
		MessageBoxA(nullptr, nullptr, "Unknown Error", MB_OK | MB_ICONERROR | MB_DEFAULT_DESKTOP_ONLY);
	}
	return EXIT_FAILURE;
}