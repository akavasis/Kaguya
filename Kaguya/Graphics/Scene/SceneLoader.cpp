#include "pch.h"
#include "SceneLoader.h"

#include <assimp/Importer.hpp>
#include <assimp/scene.h>
#include <assimp/postprocess.h>
#include <assimp/pbrmaterial.h>

using namespace DirectX;

static Assimp::Importer Importer;

SceneLoader::SceneLoader(std::filesystem::path ExecutableFolderPath)
	: m_ExecutableFolderPath(ExecutableFolderPath)
{

}

Scene SceneLoader::LoadFromFile(const char* pPath, float Scale /*= 1.0f*/) const
{
	std::filesystem::path filePath = m_ExecutableFolderPath / pPath;
	assert(std::filesystem::exists(filePath) && "File does not exist");

	Scene Scene;
	{
		const aiScene* paiScene = Importer.ReadFile(filePath.generic_string().data(),
			aiProcess_ConvertToLeftHanded |
			aiProcessPreset_TargetRealtime_MaxQuality |
			aiProcess_GenBoundingBoxes |
			aiProcess_OptimizeMeshes |
			aiProcess_OptimizeGraph);

		if (paiScene == nullptr)
		{
			LOG_ERROR("Assimp::Importer error: {}", Importer.GetErrorString());
			assert(false && "Assimp::Importer::ReadFile failed, check Assimp::Importer::GetErrorString for more info");
		}

		std::vector<Material*> materials(paiScene->mNumMaterials);
		for (unsigned int i = 0; i < paiScene->mNumMaterials; ++i)
		{
			auto& material = Scene.AddMaterial(Material());

			const aiMaterial* paiMaterial = paiScene->mMaterials[i];
			aiString aiString;

			paiMaterial->Get(AI_MATKEY_NAME, aiString);

			material.Name = aiString.C_Str();

			auto LoadTexture = [&](TextureTypes Type)
			{
				auto aiMapTextureType = [Type]()
				{
					switch (Type)
					{
					case TextureTypes::AlbedoIdx:		return aiTextureType_DIFFUSE;
					case TextureTypes::NormalIdx:		return aiTextureType_NORMALS;
					case TextureTypes::RoughnessIdx:	return aiTextureType_DIFFUSE_ROUGHNESS;
					case TextureTypes::MetallicIdx:		return aiTextureType_METALNESS;
					case TextureTypes::EmissiveIdx:		return aiTextureType_EMISSIVE;
					default:							assert(false && "Unknown Texture Type"); return aiTextureType_UNKNOWN;
					}
				};

				aiReturn aiReturn = paiMaterial->GetTexture(aiMapTextureType(), 0, &aiString);
				if (aiReturn == aiReturn_SUCCESS)
				{
					material.Textures[Type].Path = (filePath.parent_path() / aiString.C_Str()).string();
					material.Textures[Type].Flag = TextureFlags::Disk;
				}
			};

			LoadTexture(TextureTypes::AlbedoIdx);
			LoadTexture(TextureTypes::NormalIdx);
			LoadTexture(TextureTypes::RoughnessIdx);
			LoadTexture(TextureTypes::MetallicIdx);
			LoadTexture(TextureTypes::EmissiveIdx);

			materials[i] = &material;
		}

		for (unsigned int i = 0; i < paiScene->mNumMeshes; ++i)
		{
			auto& model = Scene.AddModel(Model());

			const aiMesh* paiMesh = paiScene->mMeshes[i];

			model.Path = paiMesh->mName.C_Str();

			Mesh mesh = {};

			// Parse vertex data
			std::vector<Vertex> vertices;
			vertices.reserve(paiMesh->mNumVertices);

			// Parse vertex data
			for (unsigned int vertexIndex = 0; vertexIndex < paiMesh->mNumVertices; ++vertexIndex)
			{
				Vertex v = {};
				// Position
				v.Position = XMFLOAT3(paiMesh->mVertices[vertexIndex].x, paiMesh->mVertices[vertexIndex].y, paiMesh->mVertices[vertexIndex].z);

				// Texture coords
				if (paiMesh->HasTextureCoords(0))
					v.Texture = XMFLOAT2(paiMesh->mTextureCoords[0][vertexIndex].x, paiMesh->mTextureCoords[0][vertexIndex].y);

				// Normal
				v.Normal = XMFLOAT3(paiMesh->mNormals[vertexIndex].x, paiMesh->mNormals[vertexIndex].y, paiMesh->mNormals[vertexIndex].z);

				vertices.push_back(v);
			}

			// Parse index data
			std::vector<uint32_t> indices;
			indices.reserve(size_t(paiMesh->mNumFaces) * 3);
			for (unsigned int faceIndex = 0; faceIndex < paiMesh->mNumFaces; ++faceIndex)
			{
				const aiFace& aiFace = paiMesh->mFaces[faceIndex];
				assert(aiFace.mNumIndices == 3);

				indices.push_back(aiFace.mIndices[0]);
				indices.push_back(aiFace.mIndices[1]);
				indices.push_back(aiFace.mIndices[2]);
			}

			// Parse aabb data for mesh
			// center = 0.5 * (min + max)
			XMVECTOR min = XMVectorSet(paiMesh->mAABB.mMin.x, paiMesh->mAABB.mMin.y, paiMesh->mAABB.mMin.z, 0.0f);
			XMVECTOR max = XMVectorSet(paiMesh->mAABB.mMax.x, paiMesh->mAABB.mMax.y, paiMesh->mAABB.mMax.z, 0.0f);
			XMStoreFloat3(&mesh.BoundingBox.Center, XMVectorMultiply(XMVectorReplicate(0.5f), XMVectorAdd(min, max)));
			// extents = 0.5f (max - min)
			XMStoreFloat3(&mesh.BoundingBox.Extents, XMVectorMultiply(XMVectorReplicate(0.5f), XMVectorSubtract(max, min)));

			// Parse mesh indices
			mesh.VertexCount = vertices.size();
			mesh.BaseVertexLocation = model.Vertices.empty() ? 0 : model.Vertices.size();
			mesh.IndexCount = indices.size();
			mesh.StartIndexLocation = model.Indices.empty() ? 0 : model.Indices.size();

			// Insert data into model
			model.Vertices.insert(model.Vertices.end(), std::make_move_iterator(vertices.begin()), std::make_move_iterator(vertices.end()));
			model.Indices.insert(model.Indices.end(), std::make_move_iterator(indices.begin()), std::make_move_iterator(indices.end()));

			model.Meshes.push_back(std::move(mesh));

			BoundingBox::CreateFromPoints(model.BoundingBox, model.Vertices.size(), &model.Vertices[0].Position, sizeof(Vertex));

			auto& ModelInstance = Scene.AddModelInstance({ &model, materials[paiMesh->mMaterialIndex] });
			ModelInstance.SetScale(Scale);
		}
	}
	return Scene;
}