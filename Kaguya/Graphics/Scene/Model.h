#pragma once
#include "Vertex.h"
#include "Mesh.h"

struct Model
{
	std::string					Name;
	DirectX::BoundingBox		BoundingBox;

	std::vector<Vertex>			Vertices;
	std::vector<uint32_t>		Indices;
	std::vector<Mesh>			Meshes;

	// Comitted resource
	RenderResourceHandle		VertexResource;
	RenderResourceHandle		IndexResource;

	// Heap
	RenderResourceHandle		MeshletHeap;
	RenderResourceHandle		VertexIndexHeap;
	RenderResourceHandle		PrimitiveIndexHeap;

	// Iterator interface
	auto begin() { return Meshes.begin(); }
	auto end() { return Meshes.end(); }
};

void GenerateMeshletData(const std::vector<Vertex>& Vertices, const std::vector<uint32_t>& Indices, Mesh* pMesh);
Model CreateBox(float Width, float Height, float Depth);
Model CreateGrid(float Width, float Depth, uint32_t M, uint32_t N);