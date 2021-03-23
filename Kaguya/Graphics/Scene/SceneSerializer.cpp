#include "pch.h"
#include "SceneSerializer.h"

#include "Scene.h"
#include "Entity.h"

#include <fstream>
#include <yaml-cpp/yaml.h>

#include "../AssetManager.h"

namespace Version
{
	constexpr int Major = 1;
	constexpr int Minor = 0;
	constexpr int Revision = 0;
	const std::string String = std::to_string(Major) + "." + std::to_string(Minor) + "." + std::to_string(Revision);
}

YAML::Emitter& operator<<(YAML::Emitter& Emitter, const DirectX::XMFLOAT2& Float2)
{
	Emitter << YAML::Flow;
	Emitter << YAML::BeginSeq << Float2.x << Float2.y << YAML::EndSeq;
	return Emitter;
}

YAML::Emitter& operator<<(YAML::Emitter& Emitter, const DirectX::XMFLOAT3& Float3)
{
	Emitter << YAML::Flow;
	Emitter << YAML::BeginSeq << Float3.x << Float3.y << Float3.z << YAML::EndSeq;
	return Emitter;
}

YAML::Emitter& operator<<(YAML::Emitter& Emitter, const DirectX::XMFLOAT4& Float4)
{
	Emitter << YAML::Flow;
	Emitter << YAML::BeginSeq << Float4.x << Float4.y << Float4.z << Float4.w << YAML::EndSeq;
	return Emitter;
}

template<IsAComponent T>
YAML::Emitter& operator<<(YAML::Emitter& Emitter, const T& Component)
{
	return Emitter;
}

template<>
YAML::Emitter& operator<<(YAML::Emitter& Emitter, const Tag& Tag)
{
	Emitter << YAML::Key << "Tag";
	Emitter << YAML::BeginMap;
	{
		Emitter << YAML::Key << "Name" << YAML::Value << Tag.Name;
	}
	Emitter << YAML::EndMap;
	return Emitter;
}

template<>
YAML::Emitter& operator<<(YAML::Emitter& Emitter, const Transform& Transform)
{
	Emitter << YAML::Key << "Transform";
	Emitter << YAML::BeginMap;
	{
		Emitter << YAML::Key << "Position" << YAML::Value << Transform.Position;
		Emitter << YAML::Key << "Scale" << YAML::Value << Transform.Scale;
		Emitter << YAML::Key << "Orientation" << YAML::Value << Transform.Orientation;
	}
	Emitter << YAML::EndMap;
	return Emitter;
}

template<>
YAML::Emitter& operator<<(YAML::Emitter& Emitter, const MeshFilter& MeshFilter)
{
	Emitter << YAML::Key << "Mesh Filter";
	Emitter << YAML::BeginMap;
	{
		std::filesystem::path RelativePath = MeshFilter.Mesh ? std::filesystem::relative(MeshFilter.Mesh->Metadata.Path, Application::ExecutableFolderPath) : "NULL";

		Emitter << YAML::Key << "Name" << RelativePath.string();
	}
	Emitter << YAML::EndMap;
	return Emitter;
}

template<>
YAML::Emitter& operator<<(YAML::Emitter& Emitter, const MeshRenderer& MeshRenderer)
{
	Emitter << YAML::Key << "Mesh Renderer";
	Emitter << YAML::BeginMap;
	{
		auto& Material = MeshRenderer.Material;
		Emitter << YAML::Key << "Material";
		Emitter << YAML::BeginMap;
		{
			Emitter << YAML::Key << "BSDFType" << Material.BSDFType;

			Emitter << YAML::Key << "baseColor" << Material.baseColor;
			Emitter << YAML::Key << "metallic" << Material.metallic;
			Emitter << YAML::Key << "subsurface" << Material.subsurface;
			Emitter << YAML::Key << "specular" << Material.specular;
			Emitter << YAML::Key << "roughness" << Material.roughness;
			Emitter << YAML::Key << "specularTint" << Material.specularTint;
			Emitter << YAML::Key << "anisotropic" << Material.anisotropic;
			Emitter << YAML::Key << "sheen" << Material.sheen;
			Emitter << YAML::Key << "sheenTint" << Material.sheenTint;
			Emitter << YAML::Key << "clearcoat" << Material.clearcoat;
			Emitter << YAML::Key << "clearcoatGloss" << Material.clearcoatGloss;

			Emitter << YAML::Key << "T" << Material.T;
			Emitter << YAML::Key << "etaA" << Material.etaA;
			Emitter << YAML::Key << "etaB" << Material.etaB;
		}
		Emitter << YAML::EndMap;
	}
	Emitter << YAML::EndMap;
	return Emitter;
}

template<>
YAML::Emitter& operator<<(YAML::Emitter& Emitter, const Light& Light)
{
	Emitter << YAML::Key << "Light";
	Emitter << YAML::BeginMap;
	{
		Emitter << YAML::Key << "Type" << Light.Type;
		Emitter << YAML::Key << "I" << Light.I;
		Emitter << YAML::Key << "Width" << Light.Width;
		Emitter << YAML::Key << "Height" << Light.Height;
	}
	Emitter << YAML::EndMap;
	return Emitter;
}

namespace YAML
{
	template<>
	struct convert<DirectX::XMFLOAT2>
	{
		static auto encode(const DirectX::XMFLOAT2& Float2)
		{
			Node Node;
			Node.push_back(Float2.x);
			Node.push_back(Float2.y);
			return Node;
		}

		static bool decode(const Node& Node, DirectX::XMFLOAT2& Float2)
		{
			if (!Node.IsSequence() ||
				Node.size() != 2)
			{
				return false;
			}

			Float2.x = Node[0].as<float>();
			Float2.y = Node[1].as<float>();
			return true;
		}
	};

	template<>
	struct convert<DirectX::XMFLOAT3>
	{
		static auto encode(const DirectX::XMFLOAT3& Float3)
		{
			Node Node;
			Node.push_back(Float3.x);
			Node.push_back(Float3.y);
			Node.push_back(Float3.z);
			return Node;
		}

		static bool decode(const Node& Node, DirectX::XMFLOAT3& Float3)
		{
			if (!Node.IsSequence() ||
				Node.size() != 3)
			{
				return false;
			}

			Float3.x = Node[0].as<float>();
			Float3.y = Node[1].as<float>();
			Float3.z = Node[2].as<float>();
			return true;
		}
	};

	template<>
	struct convert<DirectX::XMFLOAT4>
	{
		static auto encode(const DirectX::XMFLOAT4& Float4)
		{
			Node Node;
			Node.push_back(Float4.x);
			Node.push_back(Float4.y);
			Node.push_back(Float4.z);
			Node.push_back(Float4.w);
			return Node;
		}

		static bool decode(const Node& Node, DirectX::XMFLOAT4& Float4)
		{
			if (!Node.IsSequence() ||
				Node.size() != 4)
			{
				return false;
			}

			Float4.x = Node[0].as<float>();
			Float4.y = Node[1].as<float>();
			Float4.z = Node[2].as<float>();
			Float4.w = Node[3].as<float>();
			return true;
		}
	};
}

static void SerializeCamera(YAML::Emitter& Emitter, const Camera& Camera)
{
	Emitter << YAML::Key << "Camera";
	Emitter << YAML::BeginMap;
	{
		DirectX::XMFLOAT2 ClippingPlanes = { Camera.NearZ, Camera.FarZ };

		Emitter << Camera.Transform;
		Emitter << YAML::Key << "Field of View" << YAML::Value << Camera.FoVY;
		Emitter << YAML::Key << "Clipping Planes" << YAML::Value << ClippingPlanes;
		Emitter << YAML::Key << "Focal Length" << YAML::Value << Camera.FocalLength;
		Emitter << YAML::Key << "Relative Aperture" << YAML::Value << Camera.RelativeAperture;
		Emitter << YAML::Key << "Shutter Time" << YAML::Value << Camera.ShutterTime;
		Emitter << YAML::Key << "Sensor Sensitivity" << YAML::Value << Camera.SensorSensitivity;
	}
	Emitter << YAML::EndMap;
}

template<IsAComponent T>
void SerializeComponent(YAML::Emitter& Emitter, Entity Entity)
{
	if (Entity.HasComponent<T>())
	{
		auto& Component = Entity.GetComponent<T>();

		Emitter << Component;
	}
}

static void SerializeEntity(YAML::Emitter& Emitter, Entity Entity)
{
	Emitter << YAML::BeginMap;
	{
		Emitter << YAML::Key << "Entity" << YAML::Value << std::to_string(typeid(Entity).hash_code());

		SerializeComponent<Tag>(Emitter, Entity);
		SerializeComponent<Transform>(Emitter, Entity);
		SerializeComponent<MeshFilter>(Emitter, Entity);
		SerializeComponent<MeshRenderer>(Emitter, Entity);
		SerializeComponent<Light>(Emitter, Entity);
	}
	Emitter << YAML::EndMap;
}

void SceneSerializer::Serialize(const std::filesystem::path& Path, Scene* pScene)
{
	YAML::Emitter Emitter;
	Emitter << YAML::BeginMap;
	{
		Emitter << YAML::Key << "Version" << YAML::Value << Version::String;
		Emitter << YAML::Key << "Scene" << YAML::Value << Path.filename().string();

		SerializeCamera(Emitter, pScene->Camera);

		Emitter << YAML::Key << "Entities" << YAML::Value << YAML::BeginSeq;
		{
			pScene->Registry.each([&](auto Handle)
			{
				Entity Entity(Handle, pScene);
				if (!Entity)
				{
					return;
				}

				SerializeEntity(Emitter, Entity);
			});
		}
		Emitter << YAML::EndSeq;
	}
	Emitter << YAML::EndMap;

	std::ofstream fout(Path);
	fout << Emitter.c_str();
}

template<IsAComponent T, typename DeserializeFunction>
void DeserializeComponent(const YAML::Node& Node, Entity* pEntity, DeserializeFunction Deserialize)
{
	if (Node)
	{
		auto& Component = pEntity->GetOrAddComponent<T>();

		Deserialize(Node, Component);
	}
}

static void DeserializeCamera(const YAML::Node& Node, Scene* pScene)
{
	auto Transform = Node["Transform"];

	Camera Camera = {};

	Camera.Transform.Position = Transform["Position"].as<DirectX::XMFLOAT3>();
	Camera.Transform.Scale = Transform["Scale"].as<DirectX::XMFLOAT3>();
	Camera.Transform.Orientation = Transform["Orientation"].as<DirectX::XMFLOAT4>();

	Camera.FoVY = Node["Field of View"].as<float>();
	Camera.AspectRatio = 1.0f;
	Camera.NearZ = Node["Clipping Planes"].as<DirectX::XMFLOAT2>().x;
	Camera.FarZ = Node["Clipping Planes"].as<DirectX::XMFLOAT2>().y;

	Camera.FocalLength = Node["Focal Length"].as<float>();
	Camera.RelativeAperture = Node["Relative Aperture"].as<float>();
	Camera.ShutterTime = Node["Shutter Time"].as<float>();
	Camera.SensorSensitivity = Node["Sensor Sensitivity"].as<float>();

	pScene->Camera = Camera;
	pScene->PreviousCamera = Camera;
}

static void DeserializeEntity(const YAML::Node& Node, Scene* pScene, std::unordered_set<std::string>* Resources)
{
	// Tag component have to exist
	auto Name = Node["Tag"]["Name"].as<std::string>();

	// Create entity after getting our tag component
	Entity Entity = pScene->CreateEntity(Name);

	// TODO: Come up with some template meta programming for serialize/deserialize
	DeserializeComponent<Transform>(Node["Transform"], &Entity, [](auto& Node, auto& Transform)
	{
		Transform.Position = Node["Position"].as<DirectX::XMFLOAT3>();
		Transform.Scale = Node["Scale"].as<DirectX::XMFLOAT3>();
		Transform.Orientation = Node["Orientation"].as<DirectX::XMFLOAT4>();
	});

	DeserializeComponent<MeshFilter>(Node["Mesh Filter"], &Entity, [&](auto& Node, auto& MeshFilter)
	{
		auto Path = Node["Name"].as<std::string>();
		Path = (Application::ExecutableFolderPath / Path).string();
		Resources->insert(Path);

		MeshFilter.MeshID = entt::hashed_string(Path.data());
	});

	DeserializeComponent<MeshRenderer>(Node["Mesh Renderer"], &Entity, [](auto& Node, MeshRenderer& MeshRenderer)
	{
		auto& Material = Node["Material"];
		MeshRenderer.Material.BSDFType = Material["BSDFType"].as<int>();

		MeshRenderer.Material.baseColor = Material["baseColor"].as<DirectX::XMFLOAT3>();
		MeshRenderer.Material.metallic = Material["metallic"].as<float>();
		MeshRenderer.Material.subsurface = Material["subsurface"].as<float>();
		MeshRenderer.Material.specular = Material["specular"].as<float>();
		MeshRenderer.Material.roughness = Material["roughness"].as<float>();
		MeshRenderer.Material.specularTint = Material["specularTint"].as<float>();
		MeshRenderer.Material.anisotropic = Material["anisotropic"].as<float>();
		MeshRenderer.Material.sheen = Material["sheen"].as<float>();
		MeshRenderer.Material.sheenTint = Material["sheenTint"].as<float>();
		MeshRenderer.Material.clearcoat = Material["clearcoat"].as<float>();
		MeshRenderer.Material.clearcoatGloss = Material["clearcoatGloss"].as<float>();

		MeshRenderer.Material.T = Material["T"].as<DirectX::XMFLOAT3>();
		MeshRenderer.Material.etaA = Material["etaA"].as<float>();
		MeshRenderer.Material.etaB = Material["etaB"].as<float>();
	});

	DeserializeComponent<Light>(Node["Light"], &Entity, [](auto& Node, auto& Light)
	{
		Light.Type = (LightType)Node["Type"].as<int>();
		Light.I = Node["I"].as<DirectX::XMFLOAT3>();
		Light.Width = Node["Width"].as<float>();
		Light.Height = Node["Height"].as<float>();
	});
}

void SceneSerializer::Deserialize(const std::filesystem::path& Path, Scene* pScene)
{
	auto& AssetManager = AssetManager::Instance();

	pScene->Clear();
	AssetManager.GetImageCache().DestroyAll();
	AssetManager.GetMeshCache().DestroyAll();

	std::ifstream fin(Path);
	std::stringstream ss;
	ss << fin.rdbuf();

	auto Data = YAML::Load(ss.str());

	if (!Data["Version"])
	{
		throw std::exception("Invalid file");
	}

	auto Version = Data["Version"].as<std::string>();
	if (Version != Version::String)
	{
		throw std::exception("Invalid version");
	}

	auto Camera = Data["Camera"];
	if (!Camera)
	{
		throw std::exception("Invalid camera");
	}

	DeserializeCamera(Camera, pScene);

	std::unordered_set<std::string> Resources;
	auto Entities = Data["Entities"];
	if (Entities)
	{
		for (auto Entity : Entities)
		{
			auto ID = Entity["Entity"].as<UINT64>();

			DeserializeEntity(Entity, pScene, &Resources);
		}
	}

	for (const auto& iter : Resources)
	{
		AssetManager.AsyncLoadMesh(iter, true);
	}
}