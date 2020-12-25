#include "pch.h"
#include "SampleScene.h"
#include "Core/Application.h"

#include "MaterialLoader.h"
#include "ModelLoader.h"

using namespace DirectX;

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

Scene RandomScene(const MaterialLoader& MaterialLoader, const ModelLoader& ModelLoader)
{
	Scene scene;

	// Materials
	auto& defaultMaterial = scene.AddMaterial(MaterialLoader.LoadMaterial(0, 0, 0, 0, 0));
	defaultMaterial.Albedo = { 0.7f, 0.7f, 0.7f };
	defaultMaterial.Model = LambertianModel;

	// Models
	auto& floor = scene.AddModel(CreateGrid(500.0f, 500.0f, 10, 10));
	auto& sphere = scene.AddModel(ModelLoader.LoadFromFile("Assets/Models/Sphere.obj"));

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

Scene CornellBox(const MaterialLoader& MaterialLoader, const ModelLoader& ModelLoader)
{
	Scene scene;
	{
		// Materials
		auto& defaultMat = scene.AddMaterial(MaterialLoader.LoadMaterial(0, 0, 0, 0, 0));
		defaultMat.Albedo = { 0.7f, 0.7f, 0.7f };
		defaultMat.Model = LambertianModel;

		auto& leftWallMat = scene.AddMaterial(MaterialLoader.LoadMaterial(0, 0, 0, 0, 0));
		leftWallMat.Albedo = { 0.7f, 0.1f, 0.1f };
		leftWallMat.Model = LambertianModel;

		auto& rightWallMat = scene.AddMaterial(MaterialLoader.LoadMaterial(0, 0, 0, 0, 0));
		rightWallMat.Albedo = { 0.1f, 0.7f, 0.1f };
		rightWallMat.Model = LambertianModel;

		auto& lightMat = scene.AddMaterial(MaterialLoader.LoadMaterial(0, 0, 0, 0, 0));
		lightMat.Albedo = { 0.0f, 0.0f, 0.0f };
		lightMat.Emissive = { 15.0f, 15.0f, 15.0f };
		lightMat.Model = DiffuseLightModel;

		// Models
		auto& box = scene.AddModel(CreateBox(1.0f, 1.0f, 1.0f));
		auto& rect = scene.AddModel(CreateGrid(10.0f, 10.0f, 2, 2));
		auto& light = scene.AddModel(CreateGrid(3.0f, 3.0f, 2, 2));

		// Model instances
		auto& leftboxInstance = scene.AddModelInstance({ &box, &defaultMat });
		leftboxInstance.Rotate(0.0f, -15._Deg, 0.0f);
		leftboxInstance.Translate(-1.5f, 3.0f, 2.5f);
		leftboxInstance.SetScale(2.75f, 6.0f, 2.75f);

		auto& rightboxInstance = scene.AddModelInstance({ &box, &defaultMat });
		rightboxInstance.Rotate(0.0f, 18._Deg, 0.0f);
		rightboxInstance.Translate(1.5f, 1.375f, -1.5f);
		rightboxInstance.SetScale(2.75f, 2.75f, 2.75f);

		auto& floorInstance = scene.AddModelInstance({ &rect, &defaultMat });
		auto& ceilingInstance = scene.AddModelInstance({ &rect, &defaultMat });
		ceilingInstance.Translate(0.0f, 10.0f, 0.0f);
		ceilingInstance.Rotate(0.0f, 0.0f, XM_PI);
		auto& backwallInstance = scene.AddModelInstance({ &rect, &defaultMat });
		backwallInstance.Translate(0.0f, 5.0f, 5.0f);
		backwallInstance.Rotate(-DirectX::XM_PIDIV2, 0.0f, 0.0f);
		auto& leftwallInstance = scene.AddModelInstance({ &rect, &leftWallMat });
		leftwallInstance.Translate(-5.0f, 5.0f, 0.0f);
		leftwallInstance.Rotate(0.0f, 0.0f, -DirectX::XM_PIDIV2);
		auto& rightwallInstance = scene.AddModelInstance({ &rect, &rightWallMat });
		rightwallInstance.Translate(+5.0f, 5.0f, 0.0f);
		rightwallInstance.Rotate(0.0f, 0.0f, DirectX::XM_PIDIV2);
		auto& lightInstance = scene.AddModelInstance({ &light, &lightMat });
		lightInstance.Translate(0.0f, 9.9f, 0.0f);
	}
	return scene;
}

Scene CornellBoxLambertianSpheres(const MaterialLoader& MaterialLoader, const ModelLoader& ModelLoader)
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
	auto& sphere = scene.AddModel(ModelLoader.LoadFromFile("Assets/Models/Sphere.obj"));

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

Scene CornellBoxGlossySpheres(const MaterialLoader& MaterialLoader, const ModelLoader& ModelLoader)
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
	auto& sphere = scene.AddModel(ModelLoader.LoadFromFile("Assets/Models/Sphere.obj"));

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

Scene PlaneWithTransparentSpheres(const MaterialLoader& MaterialLoader, const ModelLoader& ModelLoader)
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
	auto& sphere = scene.AddModel(ModelLoader.LoadFromFile("Assets/Models/Sphere.obj"));

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

Scene PlaneWithLights(const MaterialLoader& MaterialLoader, const ModelLoader& ModelLoader)
{
	Scene Scene;
	{
		auto& light1 = Scene.AddLight();
		light1.Transform.Position = { 0, 10, 0 };
		light1.Transform.Rotate(XM_PIDIV2, 0, 0);
		light1.Color = { 201.0 / 255.0, 226.0 / 255.0, 255.0 / 255.0 };
		light1.SetDimension(2, 2);
		light1.SetLuminousPower(20000);

		auto& light2 = Scene.AddLight();
		light2.Transform.Position = { 7, 5, 3 };
		light2.Transform.Rotate(XM_PI - 0.5f, XM_PIDIV4, 0);
		light2.Color = { 201.0 / 255, 226.0 / 255.0, 255.0 / 255 };
		light2.SetDimension(2, 5);
		light2.SetLuminousPower(20000);

		// Materials
		auto& defaultMat = Scene.AddMaterial(MaterialLoader.LoadMaterial(0, 0, 0, 0, 0));
		defaultMat.Albedo = { 0.7f, 0.7f, 0.7f };
		defaultMat.UseAttributeAsValues = 1;

		auto& leftWallMat = Scene.AddMaterial(MaterialLoader.LoadMaterial(0, 0, 0, 0, 0));
		leftWallMat.Albedo = { 0.7f, 0.1f, 0.1f };
		leftWallMat.UseAttributeAsValues = 1;

		auto& rightWallMat = Scene.AddMaterial(MaterialLoader.LoadMaterial(0, 0, 0, 0, 0));
		rightWallMat.Albedo = { 0.1f, 0.7f, 0.1f };
		rightWallMat.UseAttributeAsValues = 1;

		auto& keybladeMat = Scene.AddMaterial(MaterialLoader.LoadMaterial(
			"Assets/Models/keyblade/textures/albedo.dds",
			"Assets/Models/keyblade/textures/normal.dds",
			"Assets/Models/keyblade/textures/ao_r_m.dds",
			"Assets/Models/keyblade/textures/ao_r_m.dds",
			0));
		keybladeMat.TextureChannel[RoughnessIdx] = TEXTURE_CHANNEL_GREEN;
		keybladeMat.TextureChannel[MetallicIdx] = TEXTURE_CHANNEL_BLUE;

		// Models
		auto& rect = Scene.AddModel(CreateGrid(10.0f, 10.0f, 2, 2));
		auto& box = Scene.AddModel(CreateBox(1.0f, 1.0f, 1.0f));
		//auto& Griffin = Scene.AddModel(ModelLoader.LoadFromFile("Assets/Models/BlackOpsGriffin/BlackOpsGriffin.obj"));
		auto& keyblade = Scene.AddModel(ModelLoader.LoadFromFile("Assets/Models/keyblade/keyblade_pbr.fbx"));
		//auto& hexgate = Scene.AddModel(ModelLoader.LoadFromFile("Assets/Models/Hexgate/Hexgate.obj"));

		// Model instances
		auto& floorInstance = Scene.AddModelInstance({ &rect, &defaultMat });
		auto& backwallInstance = Scene.AddModelInstance({ &rect, &defaultMat });
		backwallInstance.Translate(0.0f, 5.0f, 5.0f);
		backwallInstance.Rotate(-DirectX::XM_PIDIV2, 0.0f, 0.0f);
		auto& leftwallInstance = Scene.AddModelInstance({ &rect, &leftWallMat });
		leftwallInstance.Translate(-5.0f, 5.0f, 0.0f);
		leftwallInstance.Rotate(0.0f, 0.0f, -DirectX::XM_PIDIV2);
		auto& rightwallInstance = Scene.AddModelInstance({ &rect, &rightWallMat });
		rightwallInstance.Translate(+5.0f, 5.0f, 0.0f);
		rightwallInstance.Rotate(0.0f, 0.0f, DirectX::XM_PIDIV2);

		//auto& GriffinInstance = Scene.AddModelInstance({ &Griffin, &defaultMat });
		//GriffinInstance.Translate(0, 5, 0);

		auto& keybladeInstance = Scene.AddModelInstance({ &keyblade, &keybladeMat });
		keybladeInstance.Translate(4.5f, -2.0f, 0.0f);
		keybladeInstance.Rotate(0, 90.0_Deg, 0);

		//auto& hexgateInstance = Scene.AddModelInstance({ &hexgate, &defaultMat });
		//hexgateInstance.SetScale(0.025, 0.025, 0.025);

		auto& leftboxInstance = Scene.AddModelInstance({ &box, &defaultMat });
		leftboxInstance.Rotate(0.0f, -15._Deg, 0.0f);
		leftboxInstance.Translate(-1.5f, 2.5f, 2.5f);
		leftboxInstance.SetScale(2, 5, 2);

		auto& rightboxInstance = Scene.AddModelInstance({ &box, &defaultMat });
		rightboxInstance.Rotate(0.0f, 18._Deg, 0.0f);
		rightboxInstance.Translate(1.5f, 1, -1.5f);
		rightboxInstance.SetScale(2, 2, 2);

		//// Plane
		//{
		//	auto& metal = Scene.AddMaterial(MaterialLoader.LoadMaterial(
		//		"Assets/Textures/modern-tile1/modern-tile1-albedo.dds",
		//		"Assets/Textures/modern-tile1/modern-tile1-normal-ogl.dds",
		//		"Assets/Textures/modern-tile1/modern-tile1-roughness.dds",
		//		"Assets/Textures/modern-tile1/modern-tile1-metallic.dds",
		//		0));
		//	
		//	auto& plane = Scene.AddModel(CreateGrid(50.0f, 50.0f, 2, 2));

		//	auto& floorInstance = Scene.AddModelInstance({ &plane, &metal });
		//}

		//// Hexgate
		//{
		//	auto& nullMat = Scene.AddMaterial(MaterialLoader.LoadMaterial(
		//		0,
		//		0,
		//		0,
		//		0,
		//		0));

		//	//auto& hexgate = Scene.AddModel(ModelLoader.LoadFromFile("Assets/Models/Hexgate/hexgate.obj"));
		//	//auto& box = Scene.AddModel(CreateBox(2.5, 2.5, 2.5, 1));
		//	auto& sphere = Scene.AddModel(ModelLoader.LoadFromFile("Assets/Models/Sphere1.obj"));

		//	auto& boxInstance = Scene.AddModelInstance({ &sphere, &nullMat });
		//	boxInstance.Translate(0, 2.25, -4);
		//	boxInstance.Rotate(0, 45.0_Deg, 0);
		//	boxInstance.SetScale(0.025f);

		//	//auto& hexgateInstance = Scene.AddModelInstance({ &hexgate, &nullMat });
		//	//hexgateInstance.SetScale(0.025f);
		//	//hexgateInstance.Translate(0, 2.5, -4);
		//}
	}
	return Scene;
}

Scene GenerateScene(SampleScene SampleScene)
{
	auto materialLoader = MaterialLoader(Application::ExecutableFolderPath);
	auto modelLoader = ModelLoader(Application::ExecutableFolderPath);

	switch (SampleScene)
	{
	case SampleScene::Random:						return RandomScene(materialLoader, modelLoader);
	case SampleScene::CornellBox:					return CornellBox(materialLoader, modelLoader);
	case SampleScene::CornellBoxLambertianSpheres:	return CornellBoxLambertianSpheres(materialLoader, modelLoader);
	case SampleScene::CornellBoxGlossySpheres:		return CornellBoxGlossySpheres(materialLoader, modelLoader);
	case SampleScene::PlaneWithTransparentSpheres:	return PlaneWithTransparentSpheres(materialLoader, modelLoader);

	case SampleScene::PlaneWithLights:				return PlaneWithLights(materialLoader, modelLoader);
	}
	return Scene();
}