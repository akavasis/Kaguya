#pragma once
#include "../RenderDevice.h"
#include "../Vertex.h"

struct Mesh
{
	Mesh() = default;
	Mesh(const std::string& Name);
	void RenderGui();

	std::string						Name;
	DirectX::BoundingBox			BoundingBox;

	uint32_t						IndexCount;
	uint32_t						VertexCount;

	std::vector<Vertex>				Vertices;
	std::vector<uint32_t>			Indices;

	std::shared_ptr<Resource> VertexResource;
	std::shared_ptr<Resource> IndexResource;
	std::shared_ptr<Resource> AccelerationStructure;
	BottomLevelAccelerationStructure BLAS;
};

Mesh CreateBox(float Width, float Height, float Depth);
Mesh CreateGrid(float Width, float Depth, uint32_t M, uint32_t N);