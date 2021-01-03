#include "pch.h"
#include "SampleScene.h"

using namespace DirectX;

Scene CornellBox()
{
	Scene scene;
	{
		// Materials
		auto defaultMat = LoadMaterial("Default", {}, {}, {}, {}, {});
		defaultMat.Albedo = { 0.7f, 0.7f, 0.7f };
		defaultMat.Model = LambertianModel;

		auto leftWallMat = LoadMaterial("Red", {}, {}, {}, {}, {});
		leftWallMat.Albedo = { 0.7f, 0.1f, 0.1f };
		leftWallMat.Model = LambertianModel;

		auto rightWallMat = LoadMaterial("Green", {}, {}, {}, {}, {});
		rightWallMat.Albedo = { 0.1f, 0.7f, 0.1f };
		rightWallMat.Model = LambertianModel;

		auto lightMat = LoadMaterial("Light", {}, {}, {}, {}, {});
		lightMat.Albedo = { 0.0f, 0.0f, 0.0f };
		lightMat.Emissive = { 15.0f, 15.0f, 15.0f };
		lightMat.Model = DiffuseLightModel;

		auto defaultMatIndex = scene.AddMaterial(defaultMat);
		auto leftWallMatIndex = scene.AddMaterial(leftWallMat);
		auto rightWallMatIndex = scene.AddMaterial(rightWallMat);
		auto lightMatIndex = scene.AddMaterial(lightMat);

		// Models
		auto boxIndex = scene.AddMesh(CreateBox(1.0f, 1.0f, 1.0f));
		auto rectIndex = scene.AddMesh(CreateGrid(10.0f, 10.0f, 10, 10));
		auto lightIndex = scene.AddMesh(CreateGrid(3.0f, 3.0f, 2, 2));

		// Model instances
		auto leftboxInstance = MeshInstance(boxIndex, defaultMatIndex);
		leftboxInstance.Rotate(0.0f, -15._Deg, 0.0f);
		leftboxInstance.Translate(-1.5f, 3.0f, 2.5f);
		leftboxInstance.SetScale(2.75f, 6.0f, 2.75f);

		auto rightboxInstance = MeshInstance(boxIndex, defaultMatIndex);
		rightboxInstance.Rotate(0.0f, 18._Deg, 0.0f);
		rightboxInstance.Translate(1.5f, 1.375f, -1.5f);
		rightboxInstance.SetScale(2.75f, 2.75f, 2.75f);

		auto floorInstance = MeshInstance(rectIndex, defaultMatIndex);

		auto ceilingInstance = MeshInstance(rectIndex, defaultMatIndex);
		ceilingInstance.Translate(0.0f, 10.0f, 0.0f);
		ceilingInstance.Rotate(0.0f, 0.0f, XM_PI);

		auto backwallInstance = MeshInstance(rectIndex, defaultMatIndex);
		backwallInstance.Translate(0.0f, 5.0f, 5.0f);
		backwallInstance.Rotate(-DirectX::XM_PIDIV2, 0.0f, 0.0f);

		auto leftwallInstance = MeshInstance(rectIndex, leftWallMatIndex);
		leftwallInstance.Translate(-5.0f, 5.0f, 0.0f);
		leftwallInstance.Rotate(0.0f, 0.0f, -DirectX::XM_PIDIV2);

		auto rightwallInstance = MeshInstance(rectIndex, rightWallMatIndex);
		rightwallInstance.Translate(+5.0f, 5.0f, 0.0f);
		rightwallInstance.Rotate(0.0f, 0.0f, DirectX::XM_PIDIV2);

		auto lightInstance = MeshInstance(lightIndex, lightMatIndex);
		lightInstance.Translate(0.0f, 9.9f, 0.0f);

		scene.AddMeshInstance(leftboxInstance);
		scene.AddMeshInstance(rightboxInstance);
		scene.AddMeshInstance(floorInstance);
		scene.AddMeshInstance(ceilingInstance);
		scene.AddMeshInstance(backwallInstance);
		scene.AddMeshInstance(leftwallInstance);
		scene.AddMeshInstance(rightwallInstance);
		scene.AddMeshInstance(lightInstance);
	}
	return scene;
}

Scene PlaneWithLights()
{
	Scene Scene;
	{
		auto light1 = PolygonalLight("Rect light [0]");
		light1.Transform.Position = { 0, 10, 0 };
		light1.Transform.Rotate(XM_PIDIV2, 0, 0);
		light1.Color = { 201.0 / 255.0, 226.0 / 255.0, 255.0 / 255.0 };
		light1.SetDimension(2, 2);
		light1.SetLuminousPower(20000);

		Scene.AddLight(light1);

		auto light2 = PolygonalLight("Rect light [1]");
		light2.Transform.Position = { 7, 5, 3 };
		light2.Transform.Rotate(XM_PI - 0.5f, XM_PIDIV4, 0);
		light2.Color = { 201.0 / 255, 226.0 / 255.0, 255.0 / 255 };
		light2.SetDimension(2, 5);
		light2.SetLuminousPower(20000);

		Scene.AddLight(light2);

		// Materials
		//auto& defaultMat = Scene.AddMaterial(MaterialLoader.LoadMaterial(0, 0, 0, 0, 0));
		//defaultMat.Albedo = { 0.7f, 0.7f, 0.7f };
		//defaultMat.UseAttributeAsValues = 1;

		//auto& leftWallMat = Scene.AddMaterial(MaterialLoader.LoadMaterial(0, 0, 0, 0, 0));
		//leftWallMat.Albedo = { 0.7f, 0.1f, 0.1f };
		//leftWallMat.UseAttributeAsValues = 1;

		//auto& rightWallMat = Scene.AddMaterial(MaterialLoader.LoadMaterial(0, 0, 0, 0, 0));
		//rightWallMat.Albedo = { 0.1f, 0.7f, 0.1f };
		//rightWallMat.UseAttributeAsValues = 1;

		//auto& keybladeMat = Scene.AddMaterial(MaterialLoader.LoadMaterial(
		//	"Assets/Models/keyblade/textures/albedo.dds",
		//	"Assets/Models/keyblade/textures/normal.dds",
		//	"Assets/Models/keyblade/textures/ao_r_m.dds",
		//	"Assets/Models/keyblade/textures/ao_r_m.dds",
		//	0));
		//keybladeMat.TextureChannel[RoughnessIdx] = TEXTURE_CHANNEL_GREEN;
		//keybladeMat.TextureChannel[MetallicIdx] = TEXTURE_CHANNEL_BLUE;

		//// Models
		//auto& rect = Scene.AddModel(CreateGrid(10.0f, 10.0f, 2, 2));
		//auto& box = Scene.AddModel(CreateBox(1.0f, 1.0f, 1.0f));
		////auto& Griffin = Scene.AddModel(ModelLoader.LoadFromFile("Assets/Models/BlackOpsGriffin/BlackOpsGriffin.obj"));
		//auto& keyblade = Scene.AddModel(ModelLoader.LoadFromFile("Assets/Models/keyblade/keyblade_pbr.fbx"));
		////auto& hexgate = Scene.AddModel(ModelLoader.LoadFromFile("Assets/Models/Hexgate/Hexgate.obj"));

		//// Model instances
		//auto& floorInstance = Scene.AddModelInstance({ &rect, &defaultMat });
		//auto& backwallInstance = Scene.AddModelInstance({ &rect, &defaultMat });
		//backwallInstance.Translate(0.0f, 5.0f, 5.0f);
		//backwallInstance.Rotate(-DirectX::XM_PIDIV2, 0.0f, 0.0f);
		//auto& leftwallInstance = Scene.AddModelInstance({ &rect, &leftWallMat });
		//leftwallInstance.Translate(-5.0f, 5.0f, 0.0f);
		//leftwallInstance.Rotate(0.0f, 0.0f, -DirectX::XM_PIDIV2);
		//auto& rightwallInstance = Scene.AddModelInstance({ &rect, &rightWallMat });
		//rightwallInstance.Translate(+5.0f, 5.0f, 0.0f);
		//rightwallInstance.Rotate(0.0f, 0.0f, DirectX::XM_PIDIV2);

		////auto& GriffinInstance = Scene.AddModelInstance({ &Griffin, &defaultMat });
		////GriffinInstance.Translate(0, 5, 0);

		//auto& keybladeInstance = Scene.AddModelInstance({ &keyblade, &keybladeMat });
		//keybladeInstance.Translate(4.5f, -2.0f, 0.0f);
		//keybladeInstance.Rotate(0, 90.0_Deg, 0);

		////auto& hexgateInstance = Scene.AddModelInstance({ &hexgate, &defaultMat });
		////hexgateInstance.SetScale(0.025, 0.025, 0.025);

		//auto& leftboxInstance = Scene.AddModelInstance({ &box, &defaultMat });
		//leftboxInstance.Rotate(0.0f, -15._Deg, 0.0f);
		//leftboxInstance.Translate(-1.5f, 2.5f, 2.5f);
		//leftboxInstance.SetScale(2, 5, 2);

		//auto& rightboxInstance = Scene.AddModelInstance({ &box, &defaultMat });
		//rightboxInstance.Rotate(0.0f, 18._Deg, 0.0f);
		//rightboxInstance.Translate(1.5f, 1, -1.5f);
		//rightboxInstance.SetScale(2, 2, 2);
	}
	return Scene;
}

Scene GenerateScene(SampleScene SampleScene)
{
	switch (SampleScene)
	{
	case SampleScene::CornellBox:					return CornellBox();

	case SampleScene::PlaneWithLights:				return PlaneWithLights();
	}
	return Scene();
}