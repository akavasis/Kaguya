#pragma once
#include "Math/Transform.h"

struct Meshlet
{
	uint32_t VertexCount;
	uint32_t VertexOffset;
	uint32_t PrimitiveCount;
	uint32_t PrimitiveOffset;
};

struct Mesh
{
	DirectX::BoundingBox	BoundingBox;
	uint32_t				IndexCount;
	uint32_t				StartIndexLocation;
	uint32_t				VertexCount;
	uint32_t				BaseVertexLocation;
	size_t					BottomLevelAccelerationStructureIndex;

	std::vector<Meshlet>	Meshlets;
};