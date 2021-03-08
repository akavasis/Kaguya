#include "pch.h"
#include "AsyncLoader.h"

#include <DDSTextureLoader.h>
#include <WICTextureLoader.h>

#include <assimp/Importer.hpp>
#include <assimp/scene.h>
#include <assimp/postprocess.h>
#include <assimp/pbrmaterial.h>

using namespace DirectX;

static Assimp::Importer s_Importer;

static constexpr uint32_t s_ImporterFlags =
aiProcess_ConvertToLeftHanded |
aiProcess_JoinIdenticalVertices |
aiProcess_Triangulate |
aiProcess_SortByPType |
aiProcess_GenNormals |
aiProcess_GenUVCoords |
aiProcess_OptimizeMeshes |
aiProcess_ValidateDataStructure;

AsyncImageLoader::TResourcePtr AsyncImageLoader::AsyncLoad(const TMetadata& Metadata)
{
	const auto& Path = Metadata.Path;
	const auto Extension = Path.extension().string();

	ScratchImage Image;
	if (Extension == ".dds")
	{
		ThrowIfFailed(LoadFromDDSFile(Path.c_str(), DDS_FLAGS::DDS_FLAGS_FORCE_RGB, nullptr, Image));
	}
	else if (Extension == ".tga")
	{
		ScratchImage BaseImage;
		ThrowIfFailed(LoadFromTGAFile(Path.c_str(), nullptr, BaseImage));
		ThrowIfFailed(GenerateMipMaps(*BaseImage.GetImage(0, 0, 0), TEX_FILTER_DEFAULT, 0, Image, false));
	}
	else if (Extension == ".hdr")
	{
		ScratchImage BaseImage;
		ThrowIfFailed(LoadFromHDRFile(Path.c_str(), nullptr, BaseImage));
		ThrowIfFailed(GenerateMipMaps(*BaseImage.GetImage(0, 0, 0), TEX_FILTER_DEFAULT, 0, Image, false));
	}
	else
	{
		ScratchImage BaseImage;
		ThrowIfFailed(LoadFromWICFile(Path.c_str(), WIC_FLAGS::WIC_FLAGS_FORCE_RGB, nullptr, BaseImage));
		ThrowIfFailed(GenerateMipMaps(*BaseImage.GetImage(0, 0, 0), TEX_FILTER_DEFAULT, 0, Image, false));
	}

	auto pImage = std::make_shared<Asset::Image>();
	pImage->Metadata = Metadata;

	pImage->Name = Path.filename().string();
	pImage->Image = std::move(Image);
	pImage->sRGB = Metadata.sRGB;

	LOG_INFO("{} Loaded", Path.string());

	return pImage;
}

AsyncMeshLoader::TResourcePtr AsyncMeshLoader::AsyncLoad(const TMetadata& Metadata)
{
	const auto Path = Metadata.Path.string();

	const aiScene* paiScene = s_Importer.ReadFile(Path.data(), s_ImporterFlags);

	if (!paiScene || !paiScene->HasMeshes())
	{
		LOG_ERROR("Assimp::Importer error: {}", s_Importer.GetErrorString());
		return {};
	}

	auto pMesh = std::make_shared<Asset::Mesh>();
	pMesh->Metadata = Metadata;

	pMesh->Name = Metadata.Path.filename().string();

	pMesh->Submeshes.reserve(paiScene->mNumMeshes);

	uint32_t numVertices = 0;
	uint32_t numIndices = 0;
	for (size_t m = 0; m < paiScene->mNumMeshes; ++m)
	{
		const aiMesh* paiMesh = paiScene->mMeshes[m];

		// Parse vertex data
		std::vector<Vertex> vertices;
		vertices.reserve(paiMesh->mNumVertices);

		// Parse vertex data
		for (unsigned int v = 0; v < paiMesh->mNumVertices; ++v)
		{
			Vertex& vertex = vertices.emplace_back();
			// Position
			vertex.Position = { paiMesh->mVertices[v].x, paiMesh->mVertices[v].y, paiMesh->mVertices[v].z };

			// Texture coords
			if (paiMesh->HasTextureCoords(0))
			{
				vertex.Texture = { paiMesh->mTextureCoords[0][v].x, paiMesh->mTextureCoords[0][v].y };
			}

			// Normal
			if (paiMesh->HasNormals())
			{
				vertex.Normal = { paiMesh->mNormals[v].x, paiMesh->mNormals[v].y, paiMesh->mNormals[v].z };
			}
		}

		// Parse index data
		std::vector<std::uint32_t> indices;
		indices.reserve(size_t(paiMesh->mNumFaces) * 3);
		for (unsigned int f = 0; f < paiMesh->mNumFaces; ++f)
		{
			const aiFace& aiFace = paiMesh->mFaces[f];

			indices.push_back(aiFace.mIndices[0]);
			indices.push_back(aiFace.mIndices[1]);
			indices.push_back(aiFace.mIndices[2]);
		}

		// Parse mesh indices
		Asset::Submesh& Submesh = pMesh->Submeshes.emplace_back();
		Submesh.IndexCount = indices.size();
		Submesh.StartIndexLocation = numIndices;
		Submesh.VertexCount = vertices.size();
		Submesh.BaseVertexLocation = numVertices;

		// Insert data into model if requested
		if (Metadata.KeepGeometryInRAM)
		{
			pMesh->Vertices.insert(pMesh->Vertices.end(), std::make_move_iterator(vertices.begin()), std::make_move_iterator(vertices.end()));
			pMesh->Indices.insert(pMesh->Indices.end(), std::make_move_iterator(indices.begin()), std::make_move_iterator(indices.end()));
		}

		numIndices += indices.size();
		numVertices += vertices.size();
	}

	LOG_INFO("{} Loaded", Metadata.Path.string());

	return pMesh;
}