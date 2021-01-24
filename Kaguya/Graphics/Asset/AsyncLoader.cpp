#include "pch.h"
#include "AsyncLoader.h"

#include <DDSTextureLoader.h>
#include <WICTextureLoader.h>

#include <assimp/Importer.hpp>
#include <assimp/scene.h>
#include <assimp/postprocess.h>
#include <assimp/pbrmaterial.h>

using namespace DirectX;

static Assimp::Importer Importer;

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

	const aiScene* paiScene = Importer.ReadFile(Path.data(),
		aiProcess_ConvertToLeftHanded |
		aiProcessPreset_TargetRealtime_MaxQuality |
		aiProcess_OptimizeMeshes |
		aiProcess_OptimizeGraph);

	if (paiScene == nullptr)
	{
		LOG_ERROR("Assimp::Importer error: {}", Importer.GetErrorString());
		return {};
	}

	if (paiScene->mNumMeshes != 1)
	{
		LOG_ERROR("Num Meshes is greater than 1!");
		return {};
	}

	const aiMesh* paiMesh = paiScene->mMeshes[0];

	// Parse vertex data
	std::vector<Vertex> vertices(paiMesh->mNumVertices);

	// Parse vertex data
	for (unsigned int vertexIndex = 0; vertexIndex < paiMesh->mNumVertices; ++vertexIndex)
	{
		Vertex v = {};
		// Position
		v.Position = DirectX::XMFLOAT3(paiMesh->mVertices[vertexIndex].x, paiMesh->mVertices[vertexIndex].y, paiMesh->mVertices[vertexIndex].z);

		// Texture coordinates
		if (paiMesh->HasTextureCoords(0))
		{
			v.Texture = DirectX::XMFLOAT2(paiMesh->mTextureCoords[0][vertexIndex].x, paiMesh->mTextureCoords[0][vertexIndex].y);
		}

		// Normal
		if (paiMesh->HasNormals())
		{
			v.Normal = DirectX::XMFLOAT3(paiMesh->mNormals[vertexIndex].x, paiMesh->mNormals[vertexIndex].y, paiMesh->mNormals[vertexIndex].z);
		}

		vertices[vertexIndex] = v;
	}

	// Parse index data
	std::vector<std::uint32_t> indices;
	indices.reserve(size_t(paiMesh->mNumFaces) * 3);
	for (unsigned int faceIndex = 0; faceIndex < paiMesh->mNumFaces; ++faceIndex)
	{
		const aiFace& aiFace = paiMesh->mFaces[faceIndex];
		assert(aiFace.mNumIndices == 3);

		indices.push_back(aiFace.mIndices[0]);
		indices.push_back(aiFace.mIndices[1]);
		indices.push_back(aiFace.mIndices[2]);
	}

	auto pMesh = std::make_shared<Asset::Mesh>();
	pMesh->Metadata = Metadata;

	pMesh->Name = Metadata.Path.filename().string();
	DirectX::BoundingBox::CreateFromPoints(pMesh->BoundingBox, vertices.size(), &vertices[0].Position, sizeof(Vertex));

	pMesh->VertexCount = vertices.size();
	pMesh->IndexCount = indices.size();

	if (Metadata.KeepGeometryInRAM)
	{
		pMesh->Vertices = std::move(vertices);
		pMesh->Indices = std::move(indices);
	}

	LOG_INFO("{} Loaded", Metadata.Path.string());

	return pMesh;
}