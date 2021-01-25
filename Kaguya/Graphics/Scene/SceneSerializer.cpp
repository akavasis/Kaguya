#include "pch.h"
#include "SceneSerializer.h"

#include "Entity.h"

#include <fstream>
#include <yaml-cpp/yaml.h>

#include "../ResourceManager.h"

namespace Version
{
	const int Major = 0;
	const int Minor = 0;
	const int Revision = 0;
	const std::string String = std::to_string(Major) + "." + std::to_string(Minor) + "." + std::to_string(Revision);
}

YAML::Emitter& operator<<(YAML::Emitter& Emitter, const DirectX::XMFLOAT3& Vec3)
{
	Emitter << YAML::Flow;
	Emitter << YAML::BeginSeq << Vec3.x << Vec3.y << Vec3.z << YAML::EndSeq;
	return Emitter;
}

YAML::Emitter& operator<<(YAML::Emitter& Emitter, const DirectX::XMFLOAT4& Vec4)
{
	Emitter << YAML::Flow;
	Emitter << YAML::BeginSeq << Vec4.x << Vec4.y << Vec4.z << Vec4.w << YAML::EndSeq;
	return Emitter;
}

namespace YAML
{
	template<>
	struct convert<DirectX::XMFLOAT3>
	{
		static auto encode(const DirectX::XMFLOAT3& Vec3)
		{
			Node Node;
			Node.push_back(Vec3.x);
			Node.push_back(Vec3.y);
			Node.push_back(Vec3.z);
			return Node;
		}

		static bool decode(const Node& Node, DirectX::XMFLOAT3& Vec3)
		{
			if (!Node.IsSequence() ||
				Node.size() != 3)
			{
				return false;
			}

			Vec3.x = Node[0].as<float>();
			Vec3.y = Node[1].as<float>();
			Vec3.z = Node[2].as<float>();
			return true;
		}
	};

	template<>
	struct convert<DirectX::XMFLOAT4>
	{
		static auto encode(const DirectX::XMFLOAT4& Vec4)
		{
			Node Node;
			Node.push_back(Vec4.x);
			Node.push_back(Vec4.y);
			Node.push_back(Vec4.z);
			Node.push_back(Vec4.w);
			return Node;
		}

		static bool decode(const Node& Node, DirectX::XMFLOAT4& Vec4)
		{
			if (!Node.IsSequence() ||
				Node.size() != 4)
			{
				return false;
			}

			Vec4.x = Node[0].as<float>();
			Vec4.y = Node[1].as<float>();
			Vec4.z = Node[2].as<float>();
			Vec4.w = Node[3].as<float>();
			return true;
		}
	};
}

template<class T>
concept IsAComponent = std::is_base_of_v<Component, T>;

template<IsAComponent T>
void SerializeComponent(YAML::Emitter& Emitter, Entity Entity, void(*pSerializeFunction)(YAML::Emitter&, T&))
{
	if (Entity.HasComponent<T>())
	{
		auto& Component = Entity.GetComponent<T>();

		pSerializeFunction(Emitter, Component);
	}
}

static void SerializeEntity(YAML::Emitter& Emitter, Entity Entity)
{
	Emitter << YAML::BeginMap;
	{
		Emitter << YAML::Key << "Entity" << YAML::Value << std::to_string(typeid(Entity).hash_code());

		SerializeComponent<Tag>(Emitter, Entity, [](auto& Emitter, auto& Tag)
		{
			Emitter << YAML::Key << "Tag";
			Emitter << YAML::BeginMap;
			{
				Emitter << YAML::Key << "Name" << YAML::Value << Tag.Name;
			}
			Emitter << YAML::EndMap;
		});

		SerializeComponent<Transform>(Emitter, Entity, [](auto& Emitter, auto& Transform)
		{
			Emitter << YAML::Key << "Transform";
			Emitter << YAML::BeginMap;
			{
				Emitter << YAML::Key << "Position" << YAML::Value << Transform.Position;
				Emitter << YAML::Key << "Scale" << YAML::Value << Transform.Scale;
				Emitter << YAML::Key << "Orientation" << YAML::Value << Transform.Orientation;
			}
			Emitter << YAML::EndMap;
		});

		SerializeComponent<MeshFilter>(Emitter, Entity, [](auto& Emitter, auto& MeshFilter)
		{
			Emitter << YAML::Key << "Mesh Filter";
			Emitter << YAML::BeginMap;
			{
				std::string Value = MeshFilter.Mesh ? MeshFilter.Mesh->Metadata.Path.string() : "NULL";
				Emitter << YAML::Key << "Name" << Value;
			}
			Emitter << YAML::EndMap;
		});

		SerializeComponent<MeshRenderer>(Emitter, Entity, [](auto& Emitter, auto& MeshRenderer)
		{
			Emitter << YAML::Key << "Mesh Renderer";
			Emitter << YAML::BeginMap;
			{
				auto& Material = MeshRenderer.Material;
				Emitter << YAML::Key << "Material";
				Emitter << YAML::BeginMap;
				{
					Emitter << YAML::Key << "Albedo" << Material.Albedo;
					Emitter << YAML::Key << "Emissive" << Material.Emissive;
					Emitter << YAML::Key << "Specular" << Material.Specular;
					Emitter << YAML::Key << "Refraction" << Material.Refraction;
					Emitter << YAML::Key << "Specular Chance" << Material.SpecularChance;
					Emitter << YAML::Key << "Roughness" << Material.Roughness;
					Emitter << YAML::Key << "Metallic" << Material.Metallic;
					Emitter << YAML::Key << "Fuzziness" << Material.Fuzziness;
					Emitter << YAML::Key << "Index Of Refraction" << Material.IndexOfRefraction;
					Emitter << YAML::Key << "Model" << Material.Model;
				}
				Emitter << YAML::EndMap;
			}
			Emitter << YAML::EndMap;
		});
	}
	Emitter << YAML::EndMap;
}

void SceneSerializer::Serialize(const std::filesystem::path& Path, Scene* pScene)
{
	YAML::Emitter Emitter;
	Emitter << YAML::BeginMap;
	{
		Emitter << YAML::Key << "Version" << YAML::Value << Version::String;
		Emitter << YAML::Key << "Scene" << YAML::Value << "Test Scene";
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

static void DeserializeEntity(const YAML::Node& Node, Scene* pScene, ResourceManager* pResourceManager)
{
	// Tag component have to exist
	std::string Name = Node["Tag"]["Name"].as<std::string>();

	// Create entity after getting our tag component
	Entity Entity = pScene->CreateEntity(Name);

	DeserializeComponent<Transform>(Node["Transform"], &Entity, [](auto& Node, auto& Transform)
	{
		Transform.Position = Node["Position"].as<DirectX::XMFLOAT3>();
		Transform.Scale = Node["Scale"].as<DirectX::XMFLOAT3>();
		Transform.Orientation = Node["Orientation"].as<DirectX::XMFLOAT4>();
	});

	DeserializeComponent<MeshFilter>(Node["Mesh Filter"], &Entity, [&](auto& Node, auto& MeshFilter)
	{
		auto Path = Node["Name"].as<std::string>();
		if (Path != "NULL")
		{
			pResourceManager->AsyncLoadMesh(Path, true);
		}

		MeshFilter.MeshID = entt::hashed_string(Path.data());
	});

	DeserializeComponent<MeshRenderer>(Node["Mesh Renderer"], &Entity, [](auto& Node, auto& MeshRenderer)
	{
		auto& Material = Node["Material"];
		MeshRenderer.Material.Albedo = Material["Albedo"].as<DirectX::XMFLOAT3>();
		MeshRenderer.Material.Emissive = Material["Emissive"].as<DirectX::XMFLOAT3>();
		MeshRenderer.Material.Specular = Material["Specular"].as<DirectX::XMFLOAT3>();
		MeshRenderer.Material.Refraction = Material["Refraction"].as<DirectX::XMFLOAT3>();
		MeshRenderer.Material.SpecularChance = Material["Specular Chance"].as<float>();
		MeshRenderer.Material.Roughness = Material["Roughness"].as<float>();
		MeshRenderer.Material.Metallic = Material["Metallic"].as<float>();
		MeshRenderer.Material.Fuzziness = Material["Fuzziness"].as<float>();
		MeshRenderer.Material.IndexOfRefraction = Material["Index Of Refraction"].as<float>();
		MeshRenderer.Material.Model = Material["Model"].as<int>();
	});
}

void SceneSerializer::Deserialize(const std::filesystem::path& Path, Scene* pScene, ResourceManager* pResourceManager)
{
	pScene->Clear();
	pResourceManager->GetImageCache().DestroyAll();
	pResourceManager->GetMeshCache().DestroyAll();

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

	auto Entities = Data["Entities"];
	if (Entities)
	{
		for (auto Entity : Entities)
		{
			UINT64 ID = Entity["Entity"].as<UINT64>();

			DeserializeEntity(Entity, pScene, pResourceManager);
		}
	}
}