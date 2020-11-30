#include "pch.h"
#include "ModelLoader.h"

#include <assimp/Importer.hpp>
#include <assimp/scene.h>
#include <assimp/postprocess.h>
#include <assimp/pbrmaterial.h>

static Assimp::Importer Importer;

ModelLoader::ModelLoader(std::filesystem::path ExecutableFolderPath)
	: m_ExecutableFolderPath(ExecutableFolderPath)
{

}

Model ModelLoader::LoadFromFile(const std::filesystem::path& Path) const
{
	std::filesystem::path filePath = m_ExecutableFolderPath / Path;
	assert(std::filesystem::exists(filePath) && "File does not exist");

	const aiScene* paiScene = Importer.ReadFile(filePath.generic_string().data(),
		aiProcess_ConvertToLeftHanded |
		aiProcessPreset_TargetRealtime_MaxQuality |
		aiProcess_OptimizeMeshes |
		aiProcess_OptimizeGraph);

	if (paiScene == nullptr)
	{
		LOG_ERROR("Assimp::Importer error: {}", Importer.GetErrorString());
		assert(false && "Assimp::Importer::ReadFile failed, check Assimp::Importer::GetErrorString for more info");
	}

	Model model = {};
	model.Name = filePath.string();

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
			Vertex v = {};
			// Position
			v.Position = DirectX::XMFLOAT3(paiMesh->mVertices[vertexIndex].x, paiMesh->mVertices[vertexIndex].y, paiMesh->mVertices[vertexIndex].z);

			// Texture coords
			if (paiMesh->HasTextureCoords(0))
				v.Texture = DirectX::XMFLOAT2(paiMesh->mTextureCoords[0][vertexIndex].x, paiMesh->mTextureCoords[0][vertexIndex].y);

			// Normal
			v.Normal = DirectX::XMFLOAT3(paiMesh->mNormals[vertexIndex].x, paiMesh->mNormals[vertexIndex].y, paiMesh->mNormals[vertexIndex].z);

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

		// TODO: Generate meshlet data here (however, i will export models into a custom binary format with meshlet generated)
		// Refer to this video for why: https://www.youtube.com/watch?v=CFXKTXtil34&t=1452s
		std::vector<DirectX::Meshlet> meshlets;
		std::vector<uint8_t> uniqueVertexIB;
		std::vector<DirectX::MeshletTriangle> primitiveIndices;

		assert(indices.size() % 3 == 0); // Ensure this is evenly divisible
		size_t nFaces = indices.size() / 3;
		size_t nVerts = vertices.size();

		std::unique_ptr<DirectX::XMFLOAT3[]> pos(new DirectX::XMFLOAT3[nVerts]);
		for (size_t j = 0; j < nVerts; ++j)
			pos[j] = vertices[j].Position;

		HRESULT hr = DirectX::ComputeMeshlets(indices.data(), nFaces,
			pos.get(), nVerts,
			nullptr,
			meshlets, uniqueVertexIB, primitiveIndices);

		// Copy meshlet data
		mesh.Meshlets.reserve(meshlets.size());
		for (const auto& meshlet : meshlets)
		{
			::Meshlet m = {};
			m.VertexCount = meshlet.VertCount;
			m.VertexOffset = meshlet.VertOffset;
			m.PrimitiveCount = meshlet.PrimCount;
			m.PrimitiveOffset = meshlet.PrimOffset;

			mesh.Meshlets.push_back(m);
		}

		// Copy unique vertex indices data
		assert(uniqueVertexIB.size() % sizeof(uint32_t) == 0); // Ensure this is evenly divisible
		auto pUniqueVertexIndices = reinterpret_cast<const uint32_t*>(uniqueVertexIB.data());
		size_t NumUniqueVertexIndices = uniqueVertexIB.size() / sizeof(uint32_t);

		mesh.VertexIndices.reserve(NumUniqueVertexIndices);
		for (size_t i = 0; i < NumUniqueVertexIndices; ++i)
		{
			mesh.VertexIndices.push_back(pUniqueVertexIndices[i]);
		}

		// Copy primitive indices
		mesh.PrimitiveIndices.reserve(primitiveIndices.size());
		for (const auto& primitive : primitiveIndices)
		{
			MeshletPrimitive p = {};
			p.i0 = primitive.i0;
			p.i1 = primitive.i1;
			p.i2 = primitive.i2;

			mesh.PrimitiveIndices.push_back(p);
		}

		// Parse aabb data for mesh
		DirectX::BoundingBox::CreateFromPoints(mesh.BoundingBox, vertices.size(), &vertices[0].Position, sizeof(Vertex));

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