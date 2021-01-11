#include "pch.h"
#include "Scene.h"

#include <assimp/Importer.hpp>
#include <assimp/scene.h>
#include <assimp/postprocess.h>
#include <assimp/pbrmaterial.h>

using namespace DirectX;

static Assimp::Importer Importer;

Material Scene::LoadMaterial
(
	const std::string& Name,
	std::optional<std::filesystem::path> AlbedoMapPath,
	std::optional<std::filesystem::path> NormalMapPath,
	std::optional<std::filesystem::path> RoughnessMapPath,
	std::optional<std::filesystem::path> MetallicMapPath,
	std::optional<std::filesystem::path> EmissiveMapPath
)
{
	Material Material(Name);

	auto InitTexture = [&](TextureTypes Type, const std::optional<std::filesystem::path>& Path)
	{
		if (Path)
		{
			Material.Textures[Type].Path = (Application::ExecutableFolderPath / Path.value()).string();
			assert(std::filesystem::exists(Material.Textures[Type].Path));
		}
	};

	InitTexture(TextureTypes::AlbedoIdx, AlbedoMapPath);
	InitTexture(TextureTypes::NormalIdx, NormalMapPath);
	InitTexture(TextureTypes::RoughnessIdx, RoughnessMapPath);
	InitTexture(TextureTypes::MetallicIdx, MetallicMapPath);
	InitTexture(TextureTypes::EmissiveIdx, EmissiveMapPath);

	return Material;
}

Mesh Scene::LoadMeshFromFile(const std::filesystem::path& Path)
{
	std::filesystem::path filePath = Application::ExecutableFolderPath / Path;
	assert(std::filesystem::exists(filePath) && "File does not exist");

	const aiScene* paiScene = Importer.ReadFile(filePath.generic_string().data(),
		aiProcess_ConvertToLeftHanded |
		aiProcessPreset_TargetRealtime_MaxQuality |
		aiProcess_OptimizeMeshes |
		aiProcess_OptimizeGraph);

	if (paiScene == nullptr)
	{
		LOG_ERROR("Assimp::Importer error: {}", Importer.GetErrorString());
		return Mesh("NULL");
	}

	assert(paiScene->mNumMeshes == 1);

	const aiMesh* paiMesh = paiScene->mMeshes[0];
	Mesh mesh = Mesh(filePath.filename().string());

	// Parse vertex data
	std::vector<Vertex> vertices(paiMesh->mNumVertices);

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
	mesh.BaseVertexLocation = 0;
	mesh.IndexCount = indices.size();
	mesh.StartIndexLocation = 0;

	// Insert data into model
	mesh.Vertices = std::move(vertices);
	mesh.Indices = std::move(indices);

	DirectX::BoundingBox::CreateFromPoints(mesh.BoundingBox, mesh.Vertices.size(), &mesh.Vertices[0].Position, sizeof(Vertex));
	return mesh;
}

void Scene::LoadFromFile(const std::filesystem::path& Path)
{
	// TODO: Add materials to scene
	// TODO: Add mesh instances for each mesh loaded
	std::filesystem::path filePath = Application::ExecutableFolderPath / Path;
	assert(std::filesystem::exists(filePath) && "File does not exist");

	const aiScene* paiScene = Importer.ReadFile(filePath.generic_string().data(),
		aiProcess_ConvertToLeftHanded |
		aiProcessPreset_TargetRealtime_MaxQuality |
		aiProcess_OptimizeMeshes |
		aiProcess_OptimizeGraph);

	if (paiScene == nullptr)
	{
		LOG_ERROR("Assimp::Importer error: {}", Importer.GetErrorString());
		return;
	}

	for (unsigned int i = 0; i < paiScene->mNumMeshes; ++i)
	{
		const aiMesh* paiMesh = paiScene->mMeshes[i];
		Mesh mesh = Mesh(paiMesh->mName.C_Str());

		// Parse vertex data
		std::vector<Vertex> vertices(paiMesh->mNumVertices);

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
		mesh.BaseVertexLocation = 0;
		mesh.IndexCount = indices.size();
		mesh.StartIndexLocation = 0;

		// Insert data into model
		mesh.Vertices = std::move(vertices);
		mesh.Indices = std::move(indices);

		DirectX::BoundingBox::CreateFromPoints(mesh.BoundingBox, mesh.Vertices.size(), &mesh.Vertices[0].Position, sizeof(Vertex));
		Meshes.push_back(std::move(mesh));
	}
}