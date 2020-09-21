#pragma once
#include "Math/Transform.h"

struct MeshInstance
{
	const Mesh* pMesh;
	Transform Transform;
	DirectX::BoundingBox BoundingBox;
};