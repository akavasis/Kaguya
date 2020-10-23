#include "pch.h"
#include "ModelLoader.h"

#include <assimp/Importer.hpp>
#include <assimp/scene.h>
#include <assimp/postprocess.h>
#include <assimp/pbrmaterial.h>

using namespace DirectX;

static Assimp::Importer Importer;

ModelLoader::ModelLoader(std::filesystem::path ExecutableFolderPath)
	: m_ExecutableFolderPath(ExecutableFolderPath)
{
	
}

Model ModelLoader::LoadFromFile(const char* pPath) const
{
	std::filesystem::path filePath = m_ExecutableFolderPath / pPath;
	assert(std::filesystem::exists(filePath) && "File does not exist");

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

	Model model;
	model.Path = filePath.generic_string();

	// Load meshes
	model.Meshes.reserve(paiScene->mNumMeshes);
	for (unsigned int meshIndex = 0; meshIndex < paiScene->mNumMeshes; ++meshIndex)
	{
		const aiMesh* paiMesh = paiScene->mMeshes[meshIndex];

		Mesh mesh = {};

		// Parse vertex data
		std::vector<Vertex> vertices;
		vertices.reserve(paiMesh->mNumVertices);

		// Parse vertex data
		for (unsigned int vertexIndex = 0; vertexIndex < paiMesh->mNumVertices; ++vertexIndex)
		{
			Vertex v{};
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
	}

	DirectX::BoundingBox::CreateFromPoints(model.BoundingBox, model.Vertices.size(), &model.Vertices[0].Position, sizeof(Vertex));

	return model;
}