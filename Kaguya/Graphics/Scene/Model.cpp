#include "pch.h"
#include "Model.h"

#include <assimp/Importer.hpp>
#include <assimp/scene.h>
#include <assimp/postprocess.h>
#include <assimp/pbrmaterial.h>

Model::Model()
{
}

Model::Model(const char* FileName, float Scale)
{
	m_Name = FileName;
	Assimp::Importer importer;
	const aiScene* paiScene = importer.ReadFile(FileName,
		aiProcess_ConvertToLeftHanded |
		aiProcessPreset_TargetRealtime_MaxQuality |
		aiProcess_OptimizeMeshes |
		aiProcess_OptimizeGraph);

	if (paiScene == nullptr)
	{
		throw std::exception(importer.GetErrorString());
	}

	// Load materials
	std::string rootPath = m_Name.substr(0, m_Name.rfind("/"));
	m_pMaterials.reserve(paiScene->mNumMaterials);
	for (unsigned int i = 0; i < paiScene->mNumMaterials; ++i)
	{
		std::unique_ptr<Material> pMaterial = std::make_unique<Material>();
		const aiMaterial* paiMaterial = paiScene->mMaterials[i];
		aiString aiString;

		paiMaterial->Get(AI_MATKEY_NAME, aiString);
		/*pMaterial->Name = aiString.C_Str();

		auto LoadTexture = [&](TextureType Type)
		{
			auto aiMapTextureType = [Type]()
			{
				switch (Type)
				{
				case TextureType::Albedo: return aiTextureType_DIFFUSE;
				case TextureType::Normal: return aiTextureType_NORMALS;
				case TextureType::Roughness: return aiTextureType_DIFFUSE_ROUGHNESS;
				case TextureType::Metallic: return aiTextureType_METALNESS;
				case TextureType::Emissive: return aiTextureType_EMISSIVE;
				default: throw std::exception("Type error");
				}
			};

			auto MapTextureFlag = [Type]()
			{
				switch (Type)
				{
				case TextureType::Albedo: return  Material::Flags::MATERIAL_FLAG_HAVE_ALBEDO_TEXTURE;
				case TextureType::Normal: return Material::Flags::MATERIAL_FLAG_HAVE_NORMAL_TEXTURE;
				case TextureType::Roughness: return Material::Flags::MATERIAL_FLAG_HAVE_ROUGHNESS_TEXTURE;
				case TextureType::Metallic: return Material::Flags::MATERIAL_FLAG_HAVE_METALLIC_TEXTURE;
				case TextureType::Emissive: return Material::Flags::MATERIAL_FLAG_HAVE_EMISSIVE_TEXTURE;
				default: throw std::exception("Type error");
				}
			};

			aiReturn aiR = paiMaterial->GetTexture(aiMapTextureType(), 0, &aiString);
			if (auto texture = paiScene->GetEmbeddedTexture(aiString.C_Str()))
			{
				ThrowCOMIfFailed(DirectX::LoadFromWICMemory(texture->pcData, texture->mWidth, DirectX::WIC_FLAGS_NONE, nullptr, pMaterial->ScratchImages[Type]));
				pMaterial->Flags |= MapTextureFlag();
			}
			else if (aiR == aiReturn_SUCCESS)
			{
				pMaterial->Maps[Type] = rootPath + '/' + aiString.C_Str();
				pMaterial->Flags |= MapTextureFlag();
			}
		};

		LoadTexture(TextureType::Albedo);
		LoadTexture(TextureType::Normal);
		LoadTexture(TextureType::Roughness);
		LoadTexture(TextureType::Metallic);
		LoadTexture(TextureType::Emissive);*/
		m_pMaterials.push_back(std::move(pMaterial));
	}

	m_pMeshes.reserve(paiScene->mNumMeshes);
	for (unsigned int MeshID = 0; MeshID < paiScene->mNumMeshes; ++MeshID)
	{
		const aiMesh* paiMesh = paiScene->mMeshes[MeshID];

		// Local vertices
		std::vector<Vertex> lVertices;
		lVertices.reserve(paiMesh->mNumVertices);

		// Local indices
		std::vector<UINT> lIndices;
		lIndices.reserve(size_t(paiMesh->mNumFaces) * 3);

		std::unique_ptr<Mesh> pMesh = std::make_unique<Mesh>();
		pMesh->Name = paiMesh->mName.C_Str();
		pMesh->MaterialIndex = paiMesh->mMaterialIndex;

		// BoundingBox generation
		DirectX::XMVECTOR min = DirectX::g_XMInfinity.v;
		DirectX::XMVECTOR max = DirectX::g_XMNegInfinity.v;
		// Parse vertex data
		for (unsigned int VertexID = 0; VertexID < paiMesh->mNumVertices; ++VertexID)
		{
			Vertex v;
			// Position
			v.Position = DirectX::XMFLOAT3(paiMesh->mVertices[VertexID].x * Scale, paiMesh->mVertices[VertexID].y * Scale, paiMesh->mVertices[VertexID].z * Scale);
			DirectX::XMVECTOR p = DirectX::XMLoadFloat3(&v.Position);
			min = DirectX::XMVectorMin(min, p);
			max = DirectX::XMVectorMax(max, p);

			// Normal
			v.Normal = DirectX::XMFLOAT3(paiMesh->mNormals[VertexID].x, paiMesh->mNormals[VertexID].y, paiMesh->mNormals[VertexID].z);

			// Texture coords
			if (paiMesh->HasTextureCoords(0))
				v.Texture = DirectX::XMFLOAT2(paiMesh->mTextureCoords[0][VertexID].x, paiMesh->mTextureCoords[0][VertexID].y);
			else
				v.Texture = DirectX::XMFLOAT2(0.0f, 0.0f);

			// Tangents and Bitangents
			if (paiMesh->HasTangentsAndBitangents())
				v.Tangent = DirectX::XMFLOAT3(paiMesh->mTangents[VertexID].x, paiMesh->mTangents[VertexID].y, paiMesh->mTangents[VertexID].z);
			else
				v.Tangent = DirectX::XMFLOAT3(0.0f, 0.0f, 0.0f);

			lVertices.push_back(v);
		}

		// center = 0.5 * (min + max)
		DirectX::XMStoreFloat3(&pMesh->BoundingBox.Center, DirectX::XMVectorMultiply(DirectX::XMVectorReplicate(0.5f), DirectX::XMVectorAdd(min, max)));
		// extents = 0.5f (max - min)
		DirectX::XMStoreFloat3(&pMesh->BoundingBox.Extents, DirectX::XMVectorMultiply(DirectX::XMVectorReplicate(0.5f), DirectX::XMVectorSubtract(max, min)));

		// Parse index data
		for (unsigned int FaceID = 0; FaceID < paiMesh->mNumFaces; ++FaceID)
		{
			const aiFace& Face = paiMesh->mFaces[FaceID];
			assert(Face.mNumIndices == 3);

			lIndices.push_back(Face.mIndices[0]);
			lIndices.push_back(Face.mIndices[1]);
			lIndices.push_back(Face.mIndices[2]);
		}

		// Set vertex storage location for vertices
		pMesh->VertexCount = lVertices.size();
		// Base vertex location should be the current size of the vertices before inserting local vertex data
		pMesh->BaseVertexLocation = m_Vertices.empty() ? 0 : m_Vertices.size();

		// Set vertex storage location for indices
		pMesh->IndexCount = lIndices.size();
		// Base index location should be the current size of the vertices before inserting local index data
		pMesh->StartIndexLocation = m_Indices.empty() ? 0 : m_Indices.size();

		// Insert new data
		m_Vertices.insert(m_Vertices.end(), std::make_move_iterator(lVertices.begin()), std::make_move_iterator(lVertices.end()));
		m_Indices.insert(m_Indices.end(), std::make_move_iterator(lIndices.begin()), std::make_move_iterator(lIndices.end()));

		m_pMeshes.push_back(std::move(pMesh));
	}

	INT ID = 0;
	m_pRootNode = ParseNode(ID, paiScene->mRootNode);
	DirectX::BoundingBox::CreateFromPoints(m_pRootNode->BoundingBox, m_Vertices.size(), &m_Vertices[0].Position, sizeof(Vertex));
}

Model::~Model()
{
}

std::string Model::GetName() const
{
	return m_Name;
}

const DirectX::BoundingBox& Model::GetRootAABB() const
{
	return m_pRootNode->BoundingBox;
}

const Transform& Model::GetRootTransform() const
{
	return m_pRootNode->Transform;
}

const std::vector<Vertex>& Model::GetVertices() const
{
	return m_Vertices;
}

const std::vector<UINT>& Model::GetIndices() const
{
	return m_Indices;
}

std::vector<std::unique_ptr<Mesh>>& Model::GetMeshes()
{
	return m_pMeshes;
}

const std::vector<std::unique_ptr<Mesh>>& Model::GetMeshes() const
{
	return m_pMeshes;
}

std::vector<std::unique_ptr<Material>>& Model::GetMaterials()
{
	return m_pMaterials;
}

const std::vector<std::unique_ptr<Material>>& Model::GetMaterials() const
{
	return m_pMaterials;
}

void Model::SetTransform(DirectX::FXMMATRIX M)
{
	m_pRootNode->SetTransform(M);
}

void Model::Translate(float DeltaX, float DeltaY, float DeltaZ)
{
	m_pRootNode->Translate(DeltaX, DeltaY, DeltaZ);
}

void Model::Rotate(float AngleX, float AngleY, float AngleZ)
{
	m_pRootNode->Rotate(AngleX, AngleY, AngleZ);
}

void Model::RenderImGuiWindow()
{
	if (ImGui::TreeNode(m_Name.data()))
	{
		TraverseNode(m_pRootNode.get());
		if (pSelectedNode != nullptr)
		{
			bool dirty = false;
			const auto dcheck = [&dirty](bool changed) {dirty = dirty || changed; };

			ImGui::Text("Position");
			dcheck(ImGui::SliderFloat("X", &pSelectedNode->Transform.Position.x, -100.f, +100.f));
			dcheck(ImGui::SliderFloat("Y", &pSelectedNode->Transform.Position.y, -100.f, +100.f));
			dcheck(ImGui::SliderFloat("Z", &pSelectedNode->Transform.Position.z, -100.f, +100.f));
			ImGui::Text("Orientation");
			dcheck(ImGui::SliderFloat("X-Rotation", &pSelectedNode->Transform.Orientation.x, -1.0f, +1.0f));
			dcheck(ImGui::SliderFloat("Y-Rotation", &pSelectedNode->Transform.Orientation.y, -1.0f, +1.0f));
			dcheck(ImGui::SliderFloat("Z-Rotation", &pSelectedNode->Transform.Orientation.z, -1.0f, +1.0f));
			if (dirty)
			{
				pSelectedNode->SetTransform(pSelectedNode->Transform.Matrix());
			}
		}
		ImGui::TreePop();
	}
}

std::unique_ptr<Model> Model::CreateSphereFromFile(const Material& material)
{
	auto pModel = std::make_unique<Model>("../../Engine/Assets/Models/Sphere/Sphere.obj", 1.0f);
	pModel->m_pMeshes[0]->MaterialIndex = 0;
	*pModel->m_pMaterials[0].get() = material;
	return pModel;
}

std::unique_ptr<Model> Model::CreateGrid(float Width, float Depth, UINT M, UINT N, const Material& Material)
{
	MeshData meshData;

	UINT vertexCount = M * N;
	UINT faceCount = (M - 1) * (N - 1) * 2;

	// Create the vertices.
	float halfWidth = 0.5f * Width;
	float halfDepth = 0.5f * Depth;

	float dx = Width / (N - 1);
	float dz = Depth / (M - 1);

	float du = 1.0f / (N - 1);
	float dv = 1.0f / (M - 1);

	meshData.Vertices.resize(vertexCount);
	for (UINT i = 0; i < M; ++i)
	{
		float z = halfDepth - i * dz;
		for (UINT j = 0; j < N; ++j)
		{
			float x = -halfWidth + j * dx;

			meshData.Vertices[size_t(i) * N + size_t(j)].Position = DirectX::XMFLOAT3(x, 0.0f, z);
			meshData.Vertices[size_t(i) * N + size_t(j)].Normal = DirectX::XMFLOAT3(0.0f, 1.0f, 0.0f);
			meshData.Vertices[size_t(i) * N + size_t(j)].Tangent = DirectX::XMFLOAT3(1.0f, 0.0f, 0.0f);

			// Stretch texture over grid.
			meshData.Vertices[size_t(i) * M + size_t(j)].Texture.x = j * du;
			meshData.Vertices[size_t(i) * M + size_t(j)].Texture.y = i * dv;
		}
	}

	// Create the indices.
	meshData.Indices.resize(size_t(faceCount) * 3); // 3 indices per face

	// Iterate over each quad and compute indices.
	UINT k = 0;
	for (UINT i = 0; i < M - 1; ++i)
	{
		for (UINT j = 0; j < N - 1; ++j)
		{
			meshData.Indices[k] = i * N + j;
			meshData.Indices[size_t(k) + 1] = i * N + j + 1;
			meshData.Indices[size_t(k) + 2] = (i + 1) * N + j;

			meshData.Indices[size_t(k) + 3] = (i + 1) * N + j;
			meshData.Indices[size_t(k) + 4] = i * N + j + 1;
			meshData.Indices[size_t(k) + 5] = (i + 1) * N + j + 1;

			k += 6; // next quad
		}
	}

	static UINT sGridID = 0;
	return CreateFromMeshData(meshData, std::string("Grid" + std::to_string(sGridID++)), Material);
}

std::unique_ptr<Model> Model::CreateBox(float Width, float Height, float Depth, UINT NumSubdivisions, const Material& Material)
{
	MeshData meshData;

	float w2 = 0.5f * Width;
	float h2 = 0.5f * Height;
	float d2 = 0.5f * Depth;

	Vertex v[24];
	// Fill in the front face vertex data. 
	v[0] = Vertex({ -w2, -h2, -d2 }, { 0.0f, 1.0f }, { 0.0f, 0.0f, -1.0f }, { 1.0f, 0.0f, 0.0f });
	v[1] = Vertex({ -w2, +h2, -d2 }, { 0.0f, 0.0f }, { 0.0f, 0.0f, -1.0f }, { 1.0f, 0.0f, 0.0f });
	v[2] = Vertex({ +w2, +h2, -d2 }, { 1.0f, 0.0f }, { 0.0f, 0.0f, -1.0f }, { 1.0f, 0.0f, 0.0f });
	v[3] = Vertex({ +w2, -h2, -d2 }, { 1.0f, 1.0f }, { 0.0f, 0.0f, -1.0f }, { 1.0f, 0.0f, 0.0f });

	// Fill in the back face vertex data. 
	v[4] = Vertex({ -w2, -h2, +d2 }, { 1.0f, 1.0f }, { 0.0f, 0.0f, 1.0f }, { -1.0f, 0.0f, 0.0f });
	v[5] = Vertex({ +w2, -h2, +d2 }, { 0.0f, 1.0f }, { 0.0f, 0.0f, 1.0f }, { -1.0f, 0.0f, 0.0f });
	v[6] = Vertex({ +w2, +h2, +d2 }, { 0.0f, 0.0f }, { 0.0f, 0.0f, 1.0f }, { -1.0f, 0.0f, 0.0f });
	v[7] = Vertex({ -w2, +h2, +d2 }, { 1.0f, 0.0f }, { 0.0f, 0.0f, 1.0f }, { -1.0f, 0.0f, 0.0f });

	// Fill in the top face vertex data. 
	v[8] = Vertex({ -w2, +h2, -d2 }, { 0.0f, 1.0f }, { 0.0f, 1.0f, 0.0f }, { 1.0f, 0.0f, 0.0f });
	v[9] = Vertex({ -w2, +h2, +d2 }, { 0.0f, 0.0f }, { 0.0f, 1.0f, 0.0f }, { 1.0f, 0.0f, 0.0f });
	v[10] = Vertex({ +w2, +h2, +d2 }, { 1.0f, 0.0f }, { 0.0f, 1.0f, 0.0f }, { 1.0f, 0.0f, 0.0f });
	v[11] = Vertex({ +w2, +h2, -d2 }, { 1.0f, 1.0f }, { 0.0f, 1.0f, 0.0f }, { 1.0f, 0.0f, 0.0f });

	// Fill in the bottom face vertex data. 
	v[12] = Vertex({ -w2, -h2, -d2 }, { 1.0f, 1.0f }, { 0.0f, -1.0f, 0.0f }, { -1.0f, 0.0f, 0.0f });
	v[13] = Vertex({ +w2, -h2, -d2 }, { 0.0f, 1.0f }, { 0.0f, -1.0f, 0.0f }, { -1.0f, 0.0f, 0.0f });
	v[14] = Vertex({ +w2, -h2, +d2 }, { 0.0f, 0.0f }, { 0.0f, -1.0f, 0.0f }, { -1.0f, 0.0f, 0.0f });
	v[15] = Vertex({ -w2, -h2, +d2 }, { 1.0f, 0.0f }, { 0.0f, -1.0f, 0.0f }, { -1.0f, 0.0f, 0.0f });

	// Fill in the left face vertex data. 
	v[16] = Vertex({ -w2, -h2, +d2 }, { 0.0f, 1.0f }, { -1.0f, 0.0f, 0.0f }, { 0.0f, 0.0f, -1.0f });
	v[17] = Vertex({ -w2, +h2, +d2 }, { 0.0f, 0.0f }, { -1.0f, 0.0f, 0.0f }, { 0.0f, 0.0f, -1.0f });
	v[18] = Vertex({ -w2, +h2, -d2 }, { 1.0f, 0.0f }, { -1.0f, 0.0f, 0.0f }, { 0.0f, 0.0f, -1.0f });
	v[19] = Vertex({ -w2, -h2, -d2 }, { 1.0f, 1.0f }, { -1.0f, 0.0f, 0.0f }, { 0.0f, 0.0f, -1.0f });

	// Fill in the right face vertex data. 
	v[20] = Vertex({ +w2, -h2, -d2 }, { 0.0f, 1.0f }, { 1.0f, 0.0f, 0.0f }, { 0.0f, 0.0f, 1.0f });
	v[21] = Vertex({ +w2, +h2, -d2 }, { 0.0f, 0.0f }, { 1.0f, 0.0f, 0.0f }, { 0.0f, 0.0f, 1.0f });
	v[22] = Vertex({ +w2, +h2, +d2 }, { 1.0f, 0.0f }, { 1.0f, 0.0f, 0.0f }, { 0.0f, 0.0f, 1.0f });
	v[23] = Vertex({ +w2, -h2, +d2 }, { 1.0f, 1.0f }, { 1.0f, 0.0f, 0.0f }, { 0.0f, 0.0f, 1.0f });

	meshData.Vertices.assign(&v[0], &v[24]);

	unsigned int i[36];
	// Fill in the front face index data 
	i[0] = 0; i[1] = 1; i[2] = 2;
	i[3] = 0; i[4] = 2; i[5] = 3;

	// Fill in the back face index data 
	i[6] = 4; i[7] = 5; i[8] = 6;
	i[9] = 4; i[10] = 6; i[11] = 7;

	// Fill in the top face index data 
	i[12] = 8; i[13] = 9; i[14] = 10;
	i[15] = 8; i[16] = 10; i[17] = 11;

	// Fill in the bottom face index data 
	i[18] = 12; i[19] = 13; i[20] = 14;
	i[21] = 12; i[22] = 14; i[23] = 15;

	// Fill in the left face index data 
	i[24] = 16; i[25] = 17; i[26] = 18;
	i[27] = 16; i[28] = 18; i[29] = 19;

	// Fill in the right face index data 
	i[30] = 20; i[31] = 21; i[32] = 22;
	i[33] = 20; i[34] = 22; i[35] = 23;

	meshData.Indices.assign(&i[0], &i[36]);

	// Put a cap on the number of subdivisions. 
	NumSubdivisions = std::min<UINT>(NumSubdivisions, 6u);

	for (UINT i = 0; i < NumSubdivisions; ++i)
		Subdivide(meshData);

	static UINT sBoxID = 0;
	return CreateFromMeshData(meshData, std::string("Box" + std::to_string(sBoxID++)), Material);
}

std::unique_ptr<Node> Model::ParseNode(INT& ID, const aiNode* paiNode)
{
	unsigned int i;
	std::vector<Mesh*> pMeshes;
	for (i = 0; i < paiNode->mNumMeshes; ++i)
	{
		const unsigned int MeshID = paiNode->mMeshes[i];
		pMeshes.push_back(m_pMeshes.at(MeshID).get());
	}

	std::unique_ptr<Node> pNode = std::make_unique<Node>();
	pNode->pMeshes = std::move(pMeshes);
	pNode->ID = ID++;
	pNode->Name = paiNode->mName.C_Str();
	for (i = 0; i < paiNode->mNumChildren; ++i)
	{
		pNode->AddChild(ParseNode(ID, paiNode->mChildren[i]));
	}
	return pNode;
}

void Model::TraverseNode(Node* pNode)
{
	if (PushNode(pNode))
	{
		for (auto& Child : pNode->pChildren)
		{
			TraverseNode(Child.get());
		}
		PopNode();
	}
}

bool Model::PushNode(Node* pNode)
{
	// if there is no selected node, set selectedId to an impossible value 
	const int selectedId = (pSelectedNode == nullptr) ? -1 : pSelectedNode->ID;
	// build up flags for current node 
	const auto node_flags = ImGuiTreeNodeFlags_OpenOnArrow
		| ((pNode->ID == selectedId) ? ImGuiTreeNodeFlags_Selected : 0)
		| (pNode->pChildren.size() > 0 ? 0 : ImGuiTreeNodeFlags_Leaf);
	// render this node 
	const auto expanded = ImGui::TreeNodeEx(
		(void*)(intptr_t)pNode->ID,
		node_flags, pNode->Name.data()
	);
	// processing for selecting node 
	if (ImGui::IsItemClicked())
	{
		pSelectedNode = pNode;
	}
	// signal if children should also be recursed 
	return expanded;
}

void Model::PopNode()
{
	ImGui::TreePop();
}

std::unique_ptr<Model> Model::CreateFromMeshData(MeshData& meshData, std::string name, const Material& material)
{
	std::unique_ptr<Model> pModel = std::make_unique<Model>();
	pModel->m_Name = std::move(name);
	pModel->m_pRootNode = std::make_unique<Node>();
	DirectX::BoundingBox::CreateFromPoints(pModel->m_pRootNode->BoundingBox, meshData.Vertices.size(), &meshData.Vertices[0].Position, sizeof(Vertex));
	pModel->m_Vertices = std::move(meshData.Vertices);
	pModel->m_Indices = std::move(meshData.Indices);

	// Create mesh
	std::unique_ptr<Mesh> mesh = std::make_unique<Mesh>();
	mesh->Name = pModel->m_Name + "Mesh";
	mesh->BoundingBox = pModel->m_pRootNode->BoundingBox;
	mesh->MaterialIndex = 0;
	mesh->IndexCount = pModel->m_Indices.size();
	mesh->StartIndexLocation = 0;
	mesh->VertexCount = pModel->m_Vertices.size();
	mesh->BaseVertexLocation = 0;

	// Create material
	auto pMaterial = std::make_unique<Material>();
	(*pMaterial.get()) = material;

	pModel->m_pMeshes.push_back(std::move(mesh));
	pModel->m_pMaterials.push_back(std::move(pMaterial));
	pModel->m_pRootNode->pMeshes.push_back(pModel->m_pMeshes.back().get());
	return pModel;
}

void Model::Subdivide(MeshData& meshData)
{
	// Save a copy of the input geometry. 
	MeshData inputCopy = meshData;

	meshData.Vertices.resize(0);
	meshData.Indices.resize(0);

	//       v1 
	//       * 
	//      / \ 
	//     /   \ 
	//  m0*-----*m1 
	//   / \   / \ 
	//  /   \ /   \ 
	// *-----*-----* 
	// v0    m2     v2 

	UINT NumTriangles = (UINT)inputCopy.Indices.size() / 3;
	for (UINT i = 0; i < NumTriangles; ++i)
	{
		Vertex v0 = inputCopy.Vertices[inputCopy.Indices[size_t(i) * 3 + 0]];
		Vertex v1 = inputCopy.Vertices[inputCopy.Indices[size_t(i) * 3 + 1]];
		Vertex v2 = inputCopy.Vertices[inputCopy.Indices[size_t(i) * 3 + 2]];

		// Generate the midpoints. 
		Vertex m0 = MidPoint(v0, v1);
		Vertex m1 = MidPoint(v1, v2);
		Vertex m2 = MidPoint(v0, v2);

		// Add new geometry. 
		meshData.Vertices.push_back(v0); // 0 
		meshData.Vertices.push_back(v1); // 1 
		meshData.Vertices.push_back(v2); // 2 
		meshData.Vertices.push_back(m0); // 3 
		meshData.Vertices.push_back(m1); // 4 
		meshData.Vertices.push_back(m2); // 5 

		meshData.Indices.push_back(i * 6 + 0);
		meshData.Indices.push_back(i * 6 + 3);
		meshData.Indices.push_back(i * 6 + 5);

		meshData.Indices.push_back(i * 6 + 3);
		meshData.Indices.push_back(i * 6 + 4);
		meshData.Indices.push_back(i * 6 + 5);

		meshData.Indices.push_back(i * 6 + 5);
		meshData.Indices.push_back(i * 6 + 4);
		meshData.Indices.push_back(i * 6 + 2);

		meshData.Indices.push_back(i * 6 + 3);
		meshData.Indices.push_back(i * 6 + 1);
		meshData.Indices.push_back(i * 6 + 4);
	}
}

Vertex Model::MidPoint(const Vertex& v0, const Vertex& v1)
{
	DirectX::XMVECTOR p0 = XMLoadFloat3(&v0.Position);
	DirectX::XMVECTOR p1 = XMLoadFloat3(&v1.Position);

	DirectX::XMVECTOR n0 = XMLoadFloat3(&v0.Normal);
	DirectX::XMVECTOR n1 = XMLoadFloat3(&v1.Normal);

	DirectX::XMVECTOR tan0 = XMLoadFloat3(&v0.Tangent);
	DirectX::XMVECTOR tan1 = XMLoadFloat3(&v1.Tangent);

	DirectX::XMVECTOR tex0 = XMLoadFloat2(&v0.Texture);
	DirectX::XMVECTOR tex1 = XMLoadFloat2(&v1.Texture);

	// Compute the midpoints of all the attributes.  Vectors need to be normalized 
	// since linear interpolating can make them not unit length.   
	DirectX::XMVECTOR pos = DirectX::XMVectorMultiply(DirectX::XMVectorReplicate(0.5f), DirectX::XMVectorAdd(p0, p1)); // 0.5f * (p0 + p1) 
	DirectX::XMVECTOR normal = DirectX::XMVector3Normalize(DirectX::XMVectorMultiply(DirectX::XMVectorReplicate(0.5f), DirectX::XMVectorAdd(n0, n1))); // 0.5f * (n0 + n1) 
	DirectX::XMVECTOR tangent = DirectX::XMVector3Normalize(DirectX::XMVectorMultiply(DirectX::XMVectorReplicate(0.5f), DirectX::XMVectorAdd(tan0, tan1))); // 0.5f * (tan0 + tan1) 
	DirectX::XMVECTOR tex = DirectX::XMVectorMultiply(DirectX::XMVectorReplicate(0.5f), DirectX::XMVectorAdd(tex0, tex1)); // 0.5f * (tex0 + tex1) 

	Vertex v;
	DirectX::XMStoreFloat3(&v.Position, pos);
	DirectX::XMStoreFloat3(&v.Normal, normal);
	DirectX::XMStoreFloat3(&v.Tangent, tangent);
	DirectX::XMStoreFloat2(&v.Texture, tex);
	return v;
}