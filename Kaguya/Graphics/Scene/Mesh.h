#pragma once
#include <vector>

#include "Math/Transform.h"

struct Mesh
{
	Transform Transform;
	DirectX::BoundingBox BoundingBox;
	std::uint32_t IndexCount;
	std::uint32_t StartIndexLocation;
	std::uint32_t VertexCount;
	std::uint32_t BaseVertexLocation;
	std::size_t MaterialIndex;
	std::size_t ObjectConstantsIndex;
};