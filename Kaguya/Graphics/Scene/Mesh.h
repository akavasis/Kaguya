#pragma once
#include "Math/Transform.h"

struct Mesh
{
	Transform Transform;
	DirectX::BoundingBox BoundingBox;

	INT MaterialIndex;

	UINT IndexCount;
	UINT StartIndexLocation;
	UINT VertexCount;
	UINT BaseVertexLocation;
};