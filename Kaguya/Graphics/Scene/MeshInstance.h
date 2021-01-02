#pragma once
#include "Math/Transform.h"

struct MeshInstance
{
	MeshInstance(size_t MeshIndex, size_t MaterialIndex);

	void Translate(float DeltaX, float DeltaY, float DeltaZ);
	void SetScale(float Scale);
	void SetScale(float ScaleX, float ScaleY, float ScaleZ);
	void Rotate(float AngleX, float AngleY, float AngleZ);

	size_t					MeshIndex		= -1;
	size_t					MaterialIndex	= -1;
	Transform				Transform, PreviousTransform;
	DirectX::BoundingBox	BoundingBox;
	size_t					InstanceID		= -1;
};