#include "pch.h"
#include "ModelLoader.h"

#include <assimp/Importer.hpp>
#include <assimp/scene.h>
#include <assimp/postprocess.h>
#include <assimp/pbrmaterial.h>

static Assimp::Importer importer;

ModelLoader::ModelLoader(std::filesystem::path ExecutableFolderPath)
	: m_ExecutableFolderPath(ExecutableFolderPath)
{
}

ModelLoader::~ModelLoader()
{
}

Model ModelLoader::LoadFromFile(const char* pPath, float Scale)
{
	if (!pPath)
	{
		return Model();
	}

	std::filesystem::path filePath = m_ExecutableFolderPath / pPath;
	if (!std::filesystem::exists(filePath))
	{
		CORE_ERROR("File: {} Not found", filePath.generic_string());
		return Model();
	}

	const aiScene* paiScene = importer.ReadFile(filePath.generic_string().data(),
		aiProcess_ConvertToLeftHanded |
		aiProcessPreset_TargetRealtime_MaxQuality |
		aiProcess_GenBoundingBoxes |
		aiProcess_OptimizeMeshes |
		aiProcess_OptimizeGraph);

	if (paiScene == nullptr)
	{
		CORE_ERROR("Assimp::Importer error: {}", importer.GetErrorString());
		return Model();
	}

	Model model;

	// Load embedded textures
	if (paiScene->HasTextures())
	{
		model.EmbeddedTextures.reserve(paiScene->mNumTextures);

		for (unsigned int embeddedTextureIdx = 0; embeddedTextureIdx < paiScene->mNumTextures; ++embeddedTextureIdx)
		{
			const aiTexture* paiTexture = paiScene->mTextures[embeddedTextureIdx];

			EmbeddedTexture embeddedTexture{};

			embeddedTexture.Width = paiTexture->mWidth;
			embeddedTexture.Height = paiTexture->mHeight;
			embeddedTexture.IsCompressed = paiTexture->mHeight == 0 ? true : false;
			std::string ext = paiTexture->achFormatHint;
			std::transform(ext.begin(), ext.end(), embeddedTexture.Extension.begin(),
				[](unsigned char c) { return std::tolower(c); });

			if (embeddedTexture.IsCompressed)
			{
				embeddedTexture.Data.resize(embeddedTexture.Width);
				memcpy(embeddedTexture.Data.data(), paiTexture->pcData, embeddedTexture.Width);
			}
			else
			{
				std::size_t textureArea = std::size_t(embeddedTexture.Width) * std::size_t(embeddedTexture.Height);
				embeddedTexture.Data.resize(textureArea * 4);
				for (unsigned int i = 0; i < textureArea; ++i)
				{
					embeddedTexture.Data[std::size_t(i) * 4 + 0] = paiTexture->pcData[i].r;
					embeddedTexture.Data[std::size_t(i) * 4 + 1] = paiTexture->pcData[i].g;
					embeddedTexture.Data[std::size_t(i) * 4 + 2] = paiTexture->pcData[i].b;
					embeddedTexture.Data[std::size_t(i) * 4 + 3] = paiTexture->pcData[i].a;
				}
			}

			model.EmbeddedTextures.push_back(std::move(embeddedTexture));
		}
	}

	// Load materials
	model.Materials.reserve(paiScene->mNumMaterials);
	for (unsigned int materialIdx = 0; materialIdx < paiScene->mNumMaterials; ++materialIdx)
	{
		const aiMaterial* paiMaterial = paiScene->mMaterials[materialIdx];

		Material material{};

		auto loadTexture = [&](TextureTypes Type)
		{
			auto aiMapTextureType = [Type]()
			{
				switch (Type)
				{
				case TextureTypes::Albedo: return aiTextureType_DIFFUSE;
				case TextureTypes::Normal: return aiTextureType_NORMALS;
				case TextureTypes::Roughness: return aiTextureType_DIFFUSE_ROUGHNESS;
				case TextureTypes::Metallic: return aiTextureType_METALNESS;
				case TextureTypes::Emissive: return aiTextureType_EMISSIVE;
				default: return aiTextureType_UNKNOWN;
				}
			};

			if (paiMaterial->GetTextureCount(aiMapTextureType()) > 0)
			{
				aiString path;
				paiMaterial->GetTexture(aiMapTextureType(), 0, &path);

				if (path.data[0] == '*')
				{
					material.Textures[Type].EmbeddedIdx = atoi(path.C_Str() + 1);
					material.Textures[Type].Flag = TextureFlags::Embedded;
				}
				else
				{
					material.Textures[Type].Path = filePath.parent_path() / std::string(path.C_Str());
					material.Textures[Type].Flag = TextureFlags::Disk;
				}
			}
			else
			{
				material.Textures[Type].Flag = TextureFlags::Unknown;
			}
		};

		loadTexture(TextureTypes::Albedo);
		loadTexture(TextureTypes::Normal);
		loadTexture(TextureTypes::Roughness);
		loadTexture(TextureTypes::Metallic);
		loadTexture(TextureTypes::Emissive);

		model.Materials.push_back(std::move(material));
	}

	// Load meshes
	model.Meshes.reserve(paiScene->mNumMeshes);
	for (unsigned int meshIdx = 0; meshIdx < paiScene->mNumMeshes; ++meshIdx)
	{
		const aiMesh* paiMesh = paiScene->mMeshes[meshIdx];

		Mesh mesh{};

		// Parse vertex data
		std::vector<Vertex> vertices;
		vertices.reserve(paiMesh->mNumVertices);

		// Parse vertex data
		for (unsigned int VertexID = 0; VertexID < paiMesh->mNumVertices; ++VertexID)
		{
			Vertex v{};
			// Position
			v.Position = DirectX::XMFLOAT3(paiMesh->mVertices[VertexID].x * Scale, paiMesh->mVertices[VertexID].y * Scale, paiMesh->mVertices[VertexID].z * Scale);

			// Texture coords
			if (paiMesh->HasTextureCoords(0))
				v.Texture = DirectX::XMFLOAT2(paiMesh->mTextureCoords[0][VertexID].x, paiMesh->mTextureCoords[0][VertexID].y);

			// Normal
			v.Normal = DirectX::XMFLOAT3(paiMesh->mNormals[VertexID].x, paiMesh->mNormals[VertexID].y, paiMesh->mNormals[VertexID].z);

			// Tangents and Bitangents
			if (paiMesh->HasTangentsAndBitangents())
				v.Tangent = DirectX::XMFLOAT3(paiMesh->mTangents[VertexID].x, paiMesh->mTangents[VertexID].y, paiMesh->mTangents[VertexID].z);

			vertices.push_back(v);
		}

		// Parse index data
		std::vector<std::uint32_t> indices;
		indices.reserve(size_t(paiMesh->mNumFaces) * 3);
		for (unsigned int faceIdx = 0; faceIdx < paiMesh->mNumFaces; ++faceIdx)
		{
			const aiFace& aiFace = paiMesh->mFaces[faceIdx];
			assert(aiFace.mNumIndices == 3);

			indices.push_back(aiFace.mIndices[0]);
			indices.push_back(aiFace.mIndices[1]);
			indices.push_back(aiFace.mIndices[2]);
		}

		// Parse aabb data for mesh
		// center = 0.5 * (min + max)
		DirectX::XMVECTOR min = DirectX::XMVectorSet(paiMesh->mAABB.mMin.x, paiMesh->mAABB.mMin.y, paiMesh->mAABB.mMin.z, 0.0f);
		DirectX::XMVECTOR max = DirectX::XMVectorSet(paiMesh->mAABB.mMax.x, paiMesh->mAABB.mMax.y, paiMesh->mAABB.mMax.z, 0.0f);
		DirectX::XMStoreFloat3(&mesh.BoundingBox.Center, DirectX::XMVectorMultiply(DirectX::XMVectorReplicate(0.5f), DirectX::XMVectorAdd(min, max)));
		// extents = 0.5f (max - min)
		DirectX::XMStoreFloat3(&mesh.BoundingBox.Extents, DirectX::XMVectorMultiply(DirectX::XMVectorReplicate(0.5f), DirectX::XMVectorSubtract(max, min)));

		// Parse mesh indices
		mesh.VertexCount = vertices.size();
		mesh.BaseVertexLocation = model.Vertices.empty() ? 0 : model.Vertices.size();
		mesh.IndexCount = indices.size();
		mesh.StartIndexLocation = model.Indices.empty() ? 0 : model.Indices.size();

		// Parse material index
		mesh.MaterialIdx = paiMesh->mMaterialIndex;

		// Insert data into model
		model.Vertices.insert(model.Vertices.end(), std::make_move_iterator(vertices.begin()), std::make_move_iterator(vertices.end()));
		model.Indices.insert(model.Indices.end(), std::make_move_iterator(indices.begin()), std::make_move_iterator(indices.end()));

		model.Meshes.push_back(std::move(mesh));
	}

	DirectX::BoundingBox::CreateFromPoints(model.BoundingBox, model.Vertices.size(), &model.Vertices[0].Position, sizeof(Vertex));

	// Parse file path
	model.Path = filePath.generic_string();

	return model;
}