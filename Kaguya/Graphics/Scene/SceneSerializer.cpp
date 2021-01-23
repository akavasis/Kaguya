#include "pch.h"
#include "SceneSerializer.h"

#include "Entity.h"

#include <fstream>
#include <yaml-cpp/yaml.h>

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
concept IsAComponent = std::is_base_of<Component, T>::value;

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
				std::string Value = MeshFilter.pMesh ? MeshFilter.pMesh->Name : "NULL";
				Emitter << YAML::Key << "Name" << Value;
			}
			Emitter << YAML::EndMap;
		});
	}
	Emitter << YAML::EndMap;
}

void SceneSerializer::Serialize(const std::filesystem::path& Path)
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

template<IsAComponent T>
void DeserializeComponent(const YAML::Node& Node, Entity* pEntity, void(*pDeserializeFunction)(const YAML::Node&, T&))
{
	if (Node)
	{
		auto& Component = pEntity->GetOrAddComponent<T>();

		pDeserializeFunction(Node, Component);
	}
}

static void DeserializeEntity(const YAML::Node& Node, Scene* pScene)
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
}

Scene SceneSerializer::Deserialize(const std::filesystem::path& Path)
{
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

	Scene Scene;

	auto Entities = Data["Entities"];
	if (Entities)
	{
		for (auto Entity : Entities)
		{
			UINT64 ID = Entity["Entity"].as<UINT64>();

			DeserializeEntity(Entity, &Scene);
		}
	}

	return Scene;
}