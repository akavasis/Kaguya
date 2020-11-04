#pragma once
#include "Math/Transform.h"

struct MeshInstance
{
	const Mesh*				pMesh;
	Transform				Transform, PreviousTransform;
	DirectX::BoundingBox	BoundingBox;
	size_t					InstanceID;
};