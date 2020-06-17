#pragma once
#include "Vertex.h"
#include "Math/Transform.h"

struct Mesh
{
	std::string Name;
	Transform Transform;
	DirectX::BoundingBox BoundingBox;

	INT MaterialIndex;

	UINT IndexCount;
	UINT StartIndexLocation;
	UINT VertexCount;
	UINT BaseVertexLocation;
};