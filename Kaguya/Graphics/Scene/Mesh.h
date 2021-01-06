#pragma once

#include <string>

#include "Vertex.h"
#include "Graphics/RenderResourceHandle.h"

struct Meshlet
{
	uint32_t VertexCount;
	uint32_t VertexOffset;
	uint32_t PrimitiveCount;
	uint32_t PrimitiveOffset;
};

struct MeshletPrimitive
{
	uint32_t i0 : 10;
	uint32_t i1 : 10;
	uint32_t i2 : 10;
};

struct Mesh
{
	Mesh(const std::string& Name);
	void RenderGui();

	auto begin() { return Meshlets.begin(); }
	auto end() { return Meshlets.end(); }

	std::string						Name;
	DirectX::BoundingBox			BoundingBox;

	uint32_t						IndexCount;
	uint32_t						StartIndexLocation;
	uint32_t						VertexCount;
	uint32_t						BaseVertexLocation;

	// Resolved when loaded into GPU
	size_t							BottomLevelAccelerationStructureIndex;

	std::vector<Vertex>				Vertices;
	std::vector<uint32_t>			Indices;

	std::vector<Meshlet>			Meshlets;
	std::vector<uint32_t>			VertexIndices;
	std::vector<MeshletPrimitive>	PrimitiveIndices;

	// Comitted resource
	RenderResourceHandle			VertexResource;
	RenderResourceHandle			IndexResource;

	RenderResourceHandle			MeshletResource;
	RenderResourceHandle			VertexIndexResource;
	RenderResourceHandle			PrimitiveIndexResource;
};

void GenerateMeshletData(const std::vector<Vertex>& Vertices, const std::vector<uint32_t>& Indices, Mesh* pMesh);
Mesh CreateBox(float Width, float Height, float Depth);
Mesh CreateGrid(float Width, float Depth, uint32_t M, uint32_t N);