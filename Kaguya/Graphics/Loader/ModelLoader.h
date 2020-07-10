#pragma once
#include <vector>
#include <filesystem>

#include <DirectXMath.h>

enum TextureTypes
{
	Albedo,
	Normal,
	Roughness,
	Metallic,
	Emissive,
	NumTextureTypes
};

enum class TextureFlags
{
	Unknown,
	Disk,
	Embedded
};

struct MeshData
{
	std::vector<DirectX::XMFLOAT3> Positions;
	std::vector<DirectX::XMFLOAT3> TextureCoords;
	std::vector<DirectX::XMFLOAT3> Normals;
	std::vector<DirectX::XMFLOAT3> Tangents;
	std::vector<std::uint32_t> Indices;
	DirectX::BoundingBox BoundingBox;
	std::size_t MaterialIdx;
};

struct MaterialData
{
	struct TextureData
	{
		std::string Path;
		std::size_t EmbeddedIdx;
		TextureFlags Flag;
	};
	TextureData Textures[NumTextureTypes];
};

struct EmbeddedTexture
{
	std::uint32_t Width;
	std::uint32_t Height;
	bool IsCompressed;
	std::string Extension;
	std::vector<unsigned char> Data;
};

struct ModelData
{
	std::vector<MeshData> Meshes;
	std::vector<MaterialData> Materials;
	std::vector<EmbeddedTexture> EmbeddedTextures;
};

class ModelLoader
{
public:
	ModelLoader(std::filesystem::path ExecutableFolderPath);
	virtual ~ModelLoader();

	[[nodiscard]] ModelData* LoadFromFile(const char* pPath);
private:
	std::filesystem::path m_ExecutableFolderPath;

	std::unordered_map<std::string, std::unique_ptr<ModelData>> m_ModelData;
};