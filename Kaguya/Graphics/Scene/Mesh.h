#pragma once
#include <vector>
#include "Math/Transform.h"

struct Mesh
{
	Transform Transform;
	DirectX::BoundingBox BoundingBox;
	uint32_t IndexCount;
	uint32_t StartIndexLocation;
	uint32_t VertexCount;
	uint32_t BaseVertexLocation;
	size_t BottomLevelAccelerationStructureIndex;
	size_t MaterialIndex;
	size_t ObjectConstantsIndex;
};