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

ModelData* ModelLoader::LoadFromFile(const char* pPath)
{
	if (!pPath)
	{
		return nullptr;
	}

	std::filesystem::path filePath = m_ExecutableFolderPath / pPath;
	if (!std::filesystem::exists(filePath))
	{
		CORE_ERROR("File: {} Not found", filePath.generic_string());
		return nullptr;
	}

	auto iter = m_ModelData.find(filePath.generic_string());
	if (iter != m_ModelData.end())
	{
		return iter->second.get();
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
		return nullptr;
	}

	auto modelData = std::make_unique<ModelData>();

	// Load embedded textures
	if (paiScene->HasTextures())
	{
		modelData->EmbeddedTextures.reserve(paiScene->mNumTextures);

		for (unsigned int embeddedTextureIdx = 0; embeddedTextureIdx < paiScene->mNumTextures; ++embeddedTextureIdx)
		{
			const aiTexture* paiTexture = paiScene->mTextures[embeddedTextureIdx];

			EmbeddedTexture embeddedTexture;

			embeddedTexture.Width = paiTexture->mWidth;
			embeddedTexture.Height = paiTexture->mHeight;
			embeddedTexture.IsCompressed = paiTexture->mHeight == 0 ? true : false;
			embeddedTexture.Extension = std::string(paiTexture->achFormatHint);

			if (embeddedTexture.IsCompressed)
			{
				embeddedTexture.Data.resize(embeddedTexture.Width);
				memcpy(embeddedTexture.Data.data(), paiTexture->pcData, embeddedTexture.Width);
			}
			else
			{
				std::size_t textureArea = embeddedTexture.Width * embeddedTexture.Height;
				embeddedTexture.Data.resize(textureArea * 4);
				for (unsigned int i = 0; i < textureArea; ++i)
				{
					embeddedTexture.Data[i * 4 + 0] = paiTexture->pcData[i].r;
					embeddedTexture.Data[i * 4 + 1] = paiTexture->pcData[i].g;
					embeddedTexture.Data[i * 4 + 2] = paiTexture->pcData[i].b;
					embeddedTexture.Data[i * 4 + 3] = paiTexture->pcData[i].a;
				}
			}

			modelData->EmbeddedTextures.push_back(std::move(embeddedTexture));
		}
	}

	// Load materials
	modelData->Materials.reserve(paiScene->mNumMaterials);
	for (unsigned int materialIdx = 0; materialIdx < paiScene->mNumMaterials; ++materialIdx)
	{
		const aiMaterial* paiMaterial = paiScene->mMaterials[materialIdx];

		MaterialData material;

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
					material.Textures[Type].Path = std::string(path.C_Str());
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

		modelData->Materials.push_back(std::move(material));
	}

	// Load meshes
	modelData->Meshes.reserve(paiScene->mNumMeshes);
	for (unsigned int meshIdx = 0; meshIdx < paiScene->mNumMeshes; ++meshIdx)
	{
		const aiMesh* paiMesh = paiScene->mMeshes[meshIdx];

		MeshData mesh;

		// Parse vertex data
		mesh.Positions.resize(paiMesh->mNumVertices);
		mesh.TextureCoords.resize(paiMesh->mNumVertices);
		mesh.Normals.resize(paiMesh->mNumVertices);
		mesh.Tangents.resize(paiMesh->mNumVertices);

		std::size_t byteSize = paiMesh->mNumVertices * sizeof(aiVector3D);
		memcpy(mesh.Positions.data(), paiMesh->mVertices, byteSize);

		if (paiMesh->HasTextureCoords(0))
		{
			memcpy(mesh.TextureCoords.data(), paiMesh->mTextureCoords[0], byteSize);
		}

		if (paiMesh->HasNormals())
		{
			memcpy(mesh.Normals.data(), paiMesh->mNormals, byteSize);
		}

		if (paiMesh->HasTangentsAndBitangents())
		{
			memcpy(mesh.Tangents.data(), paiMesh->mTangents, byteSize);
		}

		// Parse index data
		mesh.Indices.reserve(size_t(paiMesh->mNumFaces) * 3);
		for (unsigned int faceIdx = 0; faceIdx < paiMesh->mNumFaces; ++faceIdx)
		{
			const aiFace& aiFace = paiMesh->mFaces[faceIdx];
			assert(aiFace.mNumIndices == 3);

			mesh.Indices.push_back(aiFace.mIndices[0]);
			mesh.Indices.push_back(aiFace.mIndices[1]);
			mesh.Indices.push_back(aiFace.mIndices[2]);
		}

		// Parse aabb data
		// center = 0.5 * (min + max)
		DirectX::XMVECTOR min = DirectX::XMVectorSet(paiMesh->mAABB.mMin.x, paiMesh->mAABB.mMin.y, paiMesh->mAABB.mMin.z, 0.0f);
		DirectX::XMVECTOR max = DirectX::XMVectorSet(paiMesh->mAABB.mMax.x, paiMesh->mAABB.mMax.y, paiMesh->mAABB.mMax.z, 0.0f);
		DirectX::XMStoreFloat3(&mesh.BoundingBox.Center, DirectX::XMVectorMultiply(DirectX::XMVectorReplicate(0.5f), DirectX::XMVectorAdd(min, max)));
		// extents = 0.5f (max - min)
		DirectX::XMStoreFloat3(&mesh.BoundingBox.Extents, DirectX::XMVectorMultiply(DirectX::XMVectorReplicate(0.5f), DirectX::XMVectorSubtract(max, min)));

		// Parse material index
		mesh.MaterialIdx = paiMesh->mMaterialIndex;

		modelData->Meshes.push_back(std::move(mesh));
	}

	m_ModelData[filePath.generic_string()] = std::move(modelData);
	return m_ModelData[filePath.generic_string()].get();
}