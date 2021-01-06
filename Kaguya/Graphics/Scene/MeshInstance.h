#pragma once

#include <string>

#include "Transform.h"

struct MeshInstance
{
	MeshInstance(const std::string& Name, size_t MeshIndex, size_t MaterialIndex);
	void RenderGui(int NumMeshes, int NumMaterials);

	void Translate(float DeltaX, float DeltaY, float DeltaZ);
	void SetScale(float Scale);
	void SetScale(float ScaleX, float ScaleY, float ScaleZ);
	void Rotate(float AngleX, float AngleY, float AngleZ);

	std::string Name;
	bool		Dirty;

	size_t					MeshIndex		= -1;
	size_t					MaterialIndex	= -1;
	Transform				Transform, PreviousTransform;
	DirectX::BoundingBox	BoundingBox;
	size_t					InstanceID		= -1;
};