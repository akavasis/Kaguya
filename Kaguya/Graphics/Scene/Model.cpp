#include "pch.h"
#include "Model.h"
using namespace DirectX;

struct MeshData
{
	std::vector<Vertex> Vertices;
	std::vector<uint32_t> Indices;
};

Vertex MidPoint(const Vertex& v0, const Vertex& v1)
{
	XMVECTOR p0 = XMLoadFloat3(&v0.Position);
	XMVECTOR p1 = XMLoadFloat3(&v1.Position);

	XMVECTOR n0 = XMLoadFloat3(&v0.Normal);
	XMVECTOR n1 = XMLoadFloat3(&v1.Normal);

	XMVECTOR tex0 = XMLoadFloat2(&v0.Texture);
	XMVECTOR tex1 = XMLoadFloat2(&v1.Texture);

	// Compute the midpoints of all the attributes.  Vectors need to be normalized 
	// since linear interpolating can make them not unit length.   
	XMVECTOR pos = XMVectorReplicate(0.5f) * (p0 + p1); // 0.5f * (p0 + p1) 
	XMVECTOR normal = XMVector3Normalize(XMVectorReplicate(0.5f) * (n0 + n1)); // 0.5f * (n0 + n1) 

	XMVECTOR tex = XMVectorReplicate(0.5f) * (tex0 + tex1); // 0.5f * (tex0 + tex1) 

	Vertex v;
	XMStoreFloat3(&v.Position, pos);
	XMStoreFloat3(&v.Normal, normal);
	XMStoreFloat2(&v.Texture, tex);
	return v;
}

void Subdivide(MeshData& meshData)
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

	uint32_t NumTriangles = (uint32_t)inputCopy.Indices.size() / 3;
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

Model CreateFromMeshData(MeshData& meshData)
{
	Model model{};
	DirectX::BoundingBox::CreateFromPoints(model.BoundingBox, meshData.Vertices.size(), &meshData.Vertices[0].Position, sizeof(Vertex));
	model.Vertices = std::move(meshData.Vertices);
	model.Indices = std::move(meshData.Indices);

	// Create mesh
	Mesh mesh{};
	mesh.BoundingBox = model.BoundingBox;
	mesh.IndexCount = model.Indices.size();
	mesh.StartIndexLocation = 0;
	mesh.VertexCount = model.Vertices.size();
	mesh.BaseVertexLocation = 0;

	model.Meshes.push_back(std::move(mesh));
	return model;
}

Model CreateTriangle()
{
	MeshData meshData;

	Vertex v[3];
	v[0].Position = XMFLOAT3(0.0f, 0.25f, 0.0f);
	v[1].Position = XMFLOAT3(0.25f, -0.25f, 0.0f);
	v[2].Position = XMFLOAT3(-0.25f, -0.25f, 0.0f);

	v[0].Texture = XMFLOAT2(0.0f, 0.5f);
	v[1].Texture = XMFLOAT2(1.0f, 1.0f);
	v[2].Texture = XMFLOAT2(1.0f, 0.0f);

	v[0].Normal = v[1].Normal = v[2].Normal = XMFLOAT3(0.0f, 0.0f, -1.0f);

	meshData.Vertices.assign(&v[0], &v[2]);

	uint32_t i[3] = { 0, 1, 2 };
	meshData.Indices.assign(&i[0], &i[2]);

	return CreateFromMeshData(meshData);
}

Model CreateBox(float Width, float Height, float Depth, uint32_t NumSubdivisions)
{
	MeshData meshData;

	float w2 = 0.5f * Width;
	float h2 = 0.5f * Height;
	float d2 = 0.5f * Depth;

	Vertex v[24];
	// Fill in the front face vertex data. 
	v[0] = Vertex{ { -w2, -h2, -d2 }, { 0.0f, 1.0f }, { 0.0f, 0.0f, -1.0f } };
	v[1] = Vertex{ { -w2, +h2, -d2 }, { 0.0f, 0.0f }, { 0.0f, 0.0f, -1.0f } };
	v[2] = Vertex{ { +w2, +h2, -d2 }, { 1.0f, 0.0f }, { 0.0f, 0.0f, -1.0f } };
	v[3] = Vertex{ { +w2, -h2, -d2 }, { 1.0f, 1.0f }, { 0.0f, 0.0f, -1.0f } };

	// Fill in the back face vertex data. 
	v[4] = Vertex{ { -w2, -h2, +d2 }, { 1.0f, 1.0f }, { 0.0f, 0.0f, 1.0f } };
	v[5] = Vertex{ { +w2, -h2, +d2 }, { 0.0f, 1.0f }, { 0.0f, 0.0f, 1.0f } };
	v[6] = Vertex{ { +w2, +h2, +d2 }, { 0.0f, 0.0f }, { 0.0f, 0.0f, 1.0f } };
	v[7] = Vertex{ { -w2, +h2, +d2 }, { 1.0f, 0.0f }, { 0.0f, 0.0f, 1.0f } };

	// Fill in the top face vertex data. 
	v[8] = Vertex{ { -w2, +h2, -d2 }, { 0.0f, 1.0f }, { 0.0f, 1.0f, 0.0f } };
	v[9] = Vertex{ { -w2, +h2, +d2 }, { 0.0f, 0.0f }, { 0.0f, 1.0f, 0.0f } };
	v[10] = Vertex{ { +w2, +h2, +d2 }, { 1.0f, 0.0f }, { 0.0f, 1.0f, 0.0f } };
	v[11] = Vertex{ { +w2, +h2, -d2 }, { 1.0f, 1.0f }, { 0.0f, 1.0f, 0.0f } };

	// Fill in the bottom face vertex data. 
	v[12] = Vertex{ { -w2, -h2, -d2 }, { 1.0f, 1.0f }, { 0.0f, -1.0f, 0.0f } };
	v[13] = Vertex{ { +w2, -h2, -d2 }, { 0.0f, 1.0f }, { 0.0f, -1.0f, 0.0f } };
	v[14] = Vertex{ { +w2, -h2, +d2 }, { 0.0f, 0.0f }, { 0.0f, -1.0f, 0.0f } };
	v[15] = Vertex{ { -w2, -h2, +d2 }, { 1.0f, 0.0f }, { 0.0f, -1.0f, 0.0f } };

	// Fill in the left face vertex data. 
	v[16] = Vertex{ { -w2, -h2, +d2 }, { 0.0f, 1.0f }, { -1.0f, 0.0f, 0.0f } };
	v[17] = Vertex{ { -w2, +h2, +d2 }, { 0.0f, 0.0f }, { -1.0f, 0.0f, 0.0f } };
	v[18] = Vertex{ { -w2, +h2, -d2 }, { 1.0f, 0.0f }, { -1.0f, 0.0f, 0.0f } };
	v[19] = Vertex{ { -w2, -h2, -d2 }, { 1.0f, 1.0f }, { -1.0f, 0.0f, 0.0f } };

	// Fill in the right face vertex data. 
	v[20] = Vertex{ { +w2, -h2, -d2 }, { 0.0f, 1.0f }, { 1.0f, 0.0f, 0.0f } };
	v[21] = Vertex{ { +w2, +h2, -d2 }, { 0.0f, 0.0f }, { 1.0f, 0.0f, 0.0f } };
	v[22] = Vertex{ { +w2, +h2, +d2 }, { 1.0f, 0.0f }, { 1.0f, 0.0f, 0.0f } };
	v[23] = Vertex{ { +w2, -h2, +d2 }, { 1.0f, 1.0f }, { 1.0f, 0.0f, 0.0f } };

	meshData.Vertices.assign(&v[0], &v[24]);

	uint32_t i[36];
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
	NumSubdivisions = std::min<uint32_t>(NumSubdivisions, 6u);

	for (uint32_t i = 0; i < NumSubdivisions; ++i)
		Subdivide(meshData);

	return CreateFromMeshData(meshData);
}

Model CreateGrid(float Width, float Depth, uint32_t M, uint32_t N)
{
	MeshData meshData;

	uint32_t vertexCount = M * N;
	uint32_t faceCount = (M - 1) * (N - 1) * 2;

	// Create the vertices.
	float halfWidth = 0.5f * Width;
	float halfDepth = 0.5f * Depth;

	float dx = Width / (N - 1);
	float dz = Depth / (M - 1);

	float du = 1.0f / (N - 1);
	float dv = 1.0f / (M - 1);

	meshData.Vertices.resize(vertexCount);
	for (uint32_t i = 0; i < M; ++i)
	{
		float z = halfDepth - i * dz;
		for (uint32_t j = 0; j < N; ++j)
		{
			float x = -halfWidth + j * dx;

			meshData.Vertices[size_t(i) * N + size_t(j)].Position = DirectX::XMFLOAT3(x, 0.0f, z);
			meshData.Vertices[size_t(i) * N + size_t(j)].Normal = DirectX::XMFLOAT3(0.0f, 1.0f, 0.0f);

			// Stretch texture over grid.
			meshData.Vertices[size_t(i) * M + size_t(j)].Texture.x = j * du;
			meshData.Vertices[size_t(i) * M + size_t(j)].Texture.y = i * dv;
		}
	}

	// Create the indices.
	meshData.Indices.resize(size_t(faceCount) * 3); // 3 indices per face

	// Iterate over each quad and compute indices.
	uint32_t k = 0;
	for (uint32_t i = 0; i < M - 1; ++i)
	{
		for (uint32_t j = 0; j < N - 1; ++j)
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

	return CreateFromMeshData(meshData);
}