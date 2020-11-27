#pragma once
#include "Math/Transform.h"

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
	DirectX::BoundingBox			BoundingBox;
	uint32_t						IndexCount;
	uint32_t						StartIndexLocation;
	uint32_t						VertexCount;
	uint32_t						BaseVertexLocation;
	size_t							BottomLevelAccelerationStructureIndex;

	uint32_t						MeshletOffset;
	uint32_t						UniqueVertexIndexOffset;
	uint32_t						PrimitiveIndexOffset;

	std::vector<Meshlet>			Meshlets;
	std::vector<uint32_t>			UniqueVertexIndices;
	std::vector<MeshletPrimitive>	PrimitiveIndices;

	auto begin() { return Meshlets.begin(); }
	auto end() { return Meshlets.end(); }
};