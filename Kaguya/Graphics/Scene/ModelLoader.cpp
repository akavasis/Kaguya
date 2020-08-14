#include "pch.h"
#include "ModelLoader.h"

#include <assimp/Importer.hpp>
#include <assimp/scene.h>
#include <assimp/postprocess.h>
#include <assimp/pbrmaterial.h>

ModelLoader::ModelLoader(std::filesystem::path ExecutableFolderPath)
	: m_ExecutableFolderPath(ExecutableFolderPath)
{
}

Model ModelLoader::LoadFromFile(const char* pPath, float Scale)
{
	std::filesystem::path filePath = m_ExecutableFolderPath / pPath;
	if (!std::filesystem::exists(filePath))
	{
		CORE_ERROR("File: {} Not found", filePath.generic_string());
		return Model();
	}

	static Assimp::Importer importer;
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

	// Load meshes
	model.Meshes.reserve(paiScene->mNumMeshes);
	for (unsigned int meshIndex = 0; meshIndex < paiScene->mNumMeshes; ++meshIndex)
	{
		const aiMesh* paiMesh = paiScene->mMeshes[meshIndex];

		Mesh mesh{};

		// Parse vertex data
		std::vector<Vertex> vertices;
		vertices.reserve(paiMesh->mNumVertices);

		// Parse vertex data
		for (unsigned int vertexIndex = 0; vertexIndex < paiMesh->mNumVertices; ++vertexIndex)
		{
			Vertex v{};
			// Position
			v.Position = DirectX::XMFLOAT3(paiMesh->mVertices[vertexIndex].x * Scale, paiMesh->mVertices[vertexIndex].y * Scale, paiMesh->mVertices[vertexIndex].z * Scale);

			// Texture coords
			if (paiMesh->HasTextureCoords(0))
				v.Texture = DirectX::XMFLOAT2(paiMesh->mTextureCoords[0][vertexIndex].x, paiMesh->mTextureCoords[0][vertexIndex].y);

			// Normal
			v.Normal = DirectX::XMFLOAT3(paiMesh->mNormals[vertexIndex].x, paiMesh->mNormals[vertexIndex].y, paiMesh->mNormals[vertexIndex].z);

			// Tangents and Bitangents
			if (paiMesh->HasTangentsAndBitangents())
			{
				v.Tangent = DirectX::XMFLOAT3(paiMesh->mTangents[vertexIndex].x, paiMesh->mTangents[vertexIndex].y, paiMesh->mTangents[vertexIndex].z);
				v.Bitangent = DirectX::XMFLOAT3(paiMesh->mBitangents[vertexIndex].x, paiMesh->mBitangents[vertexIndex].y, paiMesh->mBitangents[vertexIndex].z);
			}

			vertices.push_back(v);
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
		mesh.MaterialIndex = paiMesh->mMaterialIndex;

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