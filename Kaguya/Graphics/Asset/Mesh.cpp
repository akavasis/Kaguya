#include "pch.h"
#include "Mesh.h"

using namespace DirectX;

Asset::Mesh CreateFromProceduralGeometryData(std::string Name, std::vector<Vertex> Vertices, std::vector<uint32_t> Indices)
{
	// Create mesh
	auto mesh = Asset::Mesh();
	mesh.Name = Name;
	BoundingBox::CreateFromPoints(mesh.BoundingBox, Vertices.size(), &Vertices[0].Position, sizeof(Vertex));

	mesh.IndexCount = Indices.size();
	mesh.VertexCount = Vertices.size();

	mesh.Vertices = std::move(Vertices);
	mesh.Indices = std::move(Indices);

	return mesh;
}

Asset::Mesh CreateBox(float Width, float Height, float Depth)
{
	std::vector<Vertex> Vertices;
	std::vector<uint32_t> Indices;

	float w2 = 0.5f * Width;
	float h2 = 0.5f * Height;
	float d2 = 0.5f * Depth;

	Vertex v[24] = {};
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

	Vertices.assign(&v[0], &v[24]);

	uint32_t i[36] = {};
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

	Indices.assign(&i[0], &i[36]);

	static uint32_t BOX_ID = 0;
	return CreateFromProceduralGeometryData("Box " + std::to_string(BOX_ID++), Vertices, Indices);
}

Asset::Mesh CreateGrid(float Width, float Depth, uint32_t M, uint32_t N)
{
	assert(M != 1);
	assert(N != 1);

	std::vector<Vertex> Vertices;
	std::vector<uint32_t> Indices;

	uint32_t vertexCount = M * N;
	uint32_t faceCount = (M - 1) * (N - 1) * 2;

	// Create the vertices.
	float halfWidth = 0.5f * Width;
	float halfDepth = 0.5f * Depth;

	float dx = Width / (N - 1);
	float dz = Depth / (M - 1);

	float du = 1.0f / (N - 1);
	float dv = 1.0f / (M - 1);

	Vertices.resize(vertexCount);
	for (uint32_t i = 0; i < M; ++i)
	{
		float z = halfDepth - i * dz;
		for (uint32_t j = 0; j < N; ++j)
		{
			float x = -halfWidth + j * dx;

			Vertices[size_t(i) * N + size_t(j)].Position = DirectX::XMFLOAT3(x, 0.0f, z);
			Vertices[size_t(i) * N + size_t(j)].Normal = DirectX::XMFLOAT3(0.0f, 1.0f, 0.0f);

			// Stretch texture over grid.
			Vertices[size_t(i) * M + size_t(j)].Texture.x = j * du;
			Vertices[size_t(i) * M + size_t(j)].Texture.y = i * dv;
		}
	}

	// Create the indices.
	Indices.resize(size_t(faceCount) * 3); // 3 indices per face

	// Iterate over each quad and compute indices.
	uint32_t k = 0;
	for (uint32_t i = 0; i < M - 1; ++i)
	{
		for (uint32_t j = 0; j < N - 1; ++j)
		{
			Indices[k] = i * N + j;
			Indices[size_t(k) + 1] = i * N + j + 1;
			Indices[size_t(k) + 2] = (i + 1) * N + j;

			Indices[size_t(k) + 3] = (i + 1) * N + j;
			Indices[size_t(k) + 4] = i * N + j + 1;
			Indices[size_t(k) + 5] = (i + 1) * N + j + 1;

			k += 6; // next quad
		}
	}

	static uint32_t GRID_ID = 0;
	return CreateFromProceduralGeometryData("Grid " + std::to_string(GRID_ID++), Vertices, Indices);
}