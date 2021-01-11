#include "pch.h"
#include "SampleScene.h"

using namespace DirectX;

Scene CornellBox()
{
	Scene scene;
	{
		// Materials
		auto defaultMtl = Scene::LoadMaterial("Default");
		defaultMtl.Albedo = { 0.7f, 0.7f, 0.7f };
		defaultMtl.Model = LambertianModel;

		auto leftWallMtl = Scene::LoadMaterial("Red");
		leftWallMtl.Albedo = { 0.7f, 0.1f, 0.1f };
		leftWallMtl.Model = LambertianModel;

		auto rightWallMtl = Scene::LoadMaterial("Green");
		rightWallMtl.Albedo = { 0.1f, 0.7f, 0.1f };
		rightWallMtl.Model = LambertianModel;

		auto lightMtl = Scene::LoadMaterial("Light");
		lightMtl.Emissive = { 15.0f, 15.0f, 15.0f };
		lightMtl.Model = DiffuseLightModel;

		auto defaultMtlIdx = scene.AddMaterial(defaultMtl);
		auto leftWallMtlIdx = scene.AddMaterial(leftWallMtl);
		auto rightWallMtlIdx = scene.AddMaterial(rightWallMtl);
		auto lightMtlIdx = scene.AddMaterial(lightMtl);

		// Meshes
		auto boxIndex = scene.AddMesh(CreateBox(1.0f, 1.0f, 1.0f));
		auto rectIndex = scene.AddMesh(CreateGrid(10.0f, 10.0f, 10, 10));
		auto lightIndex = scene.AddMesh(CreateGrid(3.0f, 3.0f, 2, 2));

		// Mesh instances
		auto leftboxInstance = MeshInstance("Left box", boxIndex, defaultMtlIdx);
		leftboxInstance.Rotate(0.0f, -15._Deg, 0.0f);
		leftboxInstance.Translate(-1.5f, 3.0f, 2.5f);
		leftboxInstance.SetScale(2.75f, 6.0f, 2.75f);

		auto rightboxInstance = MeshInstance("Right box", boxIndex, defaultMtlIdx);
		rightboxInstance.Rotate(0.0f, 18._Deg, 0.0f);
		rightboxInstance.Translate(1.5f, 1.375f, -1.5f);
		rightboxInstance.SetScale(2.75f, 2.75f, 2.75f);

		auto floorInstance = MeshInstance("Floor", rectIndex, defaultMtlIdx);

		auto ceilingInstance = MeshInstance("Ceiling", rectIndex, defaultMtlIdx);
		ceilingInstance.Translate(0.0f, 10.0f, 0.0f);
		ceilingInstance.Rotate(0.0f, 0.0f, XM_PI);

		auto backwallInstance = MeshInstance("Back wall", rectIndex, defaultMtlIdx);
		backwallInstance.Translate(0.0f, 5.0f, 5.0f);
		backwallInstance.Rotate(-DirectX::XM_PIDIV2, 0.0f, 0.0f);

		auto leftwallInstance = MeshInstance("Left wall", rectIndex, leftWallMtlIdx);
		leftwallInstance.Translate(-5.0f, 5.0f, 0.0f);
		leftwallInstance.Rotate(0.0f, 0.0f, -DirectX::XM_PIDIV2);

		auto rightwallInstance = MeshInstance("Right wall", rectIndex, rightWallMtlIdx);
		rightwallInstance.Translate(+5.0f, 5.0f, 0.0f);
		rightwallInstance.Rotate(0.0f, 0.0f, DirectX::XM_PIDIV2);

		auto lightInstance = MeshInstance("Light", lightIndex, lightMtlIdx);
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

Scene Hyperion()
{
	Scene scene;
	{
		auto floorMtl = Scene::LoadMaterial("Floor");
		floorMtl.Albedo = { 0.75, 0.6585, 0.5582 };

		auto tableMtl = Scene::LoadMaterial("Table");
		tableMtl.Albedo = { 0.87, 0.7785, 0.6782 };

		auto lightMat = Scene::LoadMaterial("Light");
		lightMat.Emissive = { 200.0f, 200.0f, 200.0f };
		lightMat.Model = DiffuseLightModel;

		auto glassMtl = Scene::LoadMaterial("Glass");
		glassMtl.IndexOfRefraction = 1.51714f;
		glassMtl.Model = DielectricModel;

		auto metalMtl = Scene::LoadMaterial("Metal");
		metalMtl.Albedo = { 0.3, 0.3, 0.3 };
		metalMtl.Fuzziness = 0.003f;
		metalMtl.Model = MetalModel;

		auto redMtl = Scene::LoadMaterial("Ping Pong");
		redMtl.Albedo = { 0.4, 0.2, 0.2 };
		redMtl.Fuzziness = 0.2f;
		redMtl.Model = MetalModel;

		auto ringMtl = Scene::LoadMaterial("Ring");
		ringMtl.Albedo = { 0.95, 0.93, 0.88 };
		ringMtl.Fuzziness = 0,02;
		ringMtl.Model = MetalModel;

		auto woodMtl = Scene::LoadMaterial("Wood");
		woodMtl.Albedo = { 0.3992, 0.21951971, 0.10871 };

		auto golfBallMtl = Scene::LoadMaterial("Golf Ball");
		golfBallMtl.Albedo = { 0.9, 0.87, 0.95 };

		auto floorMtlIdx = scene.AddMaterial(floorMtl);
		auto tableMtlIdx = scene.AddMaterial(tableMtl);
		auto lightMtlIdx = scene.AddMaterial(lightMat);
		auto glassMtlIdx = scene.AddMaterial(glassMtl);
		auto metalMtlIdx = scene.AddMaterial(metalMtl);
		auto redMtlIdx = scene.AddMaterial(redMtl);
		auto ringMtlIdx = scene.AddMaterial(ringMtl);
		auto woodMtlIdx = scene.AddMaterial(woodMtl);
		auto golfBallMtlIdx = scene.AddMaterial(golfBallMtl);

		auto floor = scene.AddMesh(CreateGrid(40.0f, 40.0f, 2, 2));
		auto table = scene.AddMesh(CreateBox(10, 0.4, 7));
		auto sphere = scene.AddMesh(Scene::LoadMeshFromFile("Assets/Models/Sphere.obj"));
		auto ring = scene.AddMesh(Scene::LoadMeshFromFile("Assets/Models/Ring.obj"));
		auto burrPuzzle = scene.AddMesh(Scene::LoadMeshFromFile("Assets/Models/BurrPuzzle.obj"));
		auto egg = scene.AddMesh(Scene::LoadMeshFromFile("Assets/Models/Egg.obj"));

		auto floorInstance = MeshInstance("Floor", floor, floorMtlIdx);
		auto tableInstance = MeshInstance("Table", table, tableMtlIdx);

		auto lightInstance = MeshInstance("Light", sphere, lightMtlIdx);
		lightInstance.SetScale(2);
		lightInstance.Translate(-20, 17, 0);

		auto glassInstance = MeshInstance("Glass", sphere, glassMtlIdx);
		glassInstance.SetScale(0.55);
		glassInstance.Translate(3.5, 0.75, 0);

		auto metalInstance = MeshInstance("Metal", sphere, metalMtlIdx);
		metalInstance.SetScale(0.6);
		metalInstance.Translate(-3.5, 1, 0);

		auto redBallInstance = MeshInstance("Red ball", sphere, redMtlIdx);
		redBallInstance.SetScale(0.45);
		redBallInstance.Translate(-1.5, 0.75, 1.1);

		auto ring0 = MeshInstance("Ring 0", ring, ringMtlIdx);
		ring0.SetScale(0.005f);
		ring0.Translate(0, 0.2, 0);

		auto ring1 = MeshInstance("Ring 1", ring, ringMtlIdx);
		ring1.SetScale(0.005f);
		ring1.Rotate(1.984_Deg, 6.683_Deg, -15.97_Deg);
		ring1.Translate(0.431, 0.296, 0.044);

		auto ring2 = MeshInstance("Ring 2", ring, ringMtlIdx);
		ring2.SetScale(0.005f);
		ring2.Translate(-1.3, 0.2, -0.3);

		auto burrPuzzleInstance = MeshInstance("Burr Puzzle", burrPuzzle, woodMtlIdx);
		burrPuzzleInstance.SetScale(20);
		burrPuzzleInstance.Translate(0, 1.25, 2);
		burrPuzzleInstance.Rotate(0, 30.0_Deg, 0);

		auto eggInstance = MeshInstance("Egg", egg, glassMtlIdx);
		eggInstance.Translate(2, 0.3, 1);

		scene.AddMeshInstance(floorInstance);
		scene.AddMeshInstance(tableInstance);
		scene.AddMeshInstance(lightInstance);
		scene.AddMeshInstance(glassInstance);
		scene.AddMeshInstance(metalInstance);
		scene.AddMeshInstance(redBallInstance);
		scene.AddMeshInstance(ring0);
		scene.AddMeshInstance(ring1);
		scene.AddMeshInstance(ring2);
		scene.AddMeshInstance(burrPuzzleInstance);
		scene.AddMeshInstance(eggInstance);
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
	case SampleScene::Hyperion:						return Hyperion();

	case SampleScene::PlaneWithLights:				return PlaneWithLights();
	}
	return Scene();
}