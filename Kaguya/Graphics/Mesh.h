#pragma once
#include "RenderDevice.h"
#include "Vertex.h"

struct Mesh
{
	Mesh() = default;
	Mesh(const std::string& Name);
	void RenderGui();

	std::string						Name;
	DirectX::BoundingBox			BoundingBox;

	uint32_t						IndexCount;
	uint32_t						VertexCount;

	// Resolved when loaded into GPU
	size_t							BottomLevelAccelerationStructureIndex;

	std::vector<Vertex>				Vertices;
	std::vector<uint32_t>			Indices;

	Resource						VertexResource;
	Resource						IndexResource;
};

Mesh CreateBox(float Width, float Height, float Depth);
Mesh CreateGrid(float Width, float Depth, uint32_t M, uint32_t N);