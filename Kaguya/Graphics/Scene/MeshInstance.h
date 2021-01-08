#pragma once

#include <string>

#include "Transform.h"

struct MeshInstance
{
	MeshInstance(const std::string& Name, uint32_t MeshIndex, uint32_t MaterialIndex);
	void RenderGui(uint32_t NumMeshes, uint32_t NumMaterials);

	void Translate(float DeltaX, float DeltaY, float DeltaZ);
	void SetScale(float Scale);
	void SetScale(float ScaleX, float ScaleY, float ScaleZ);
	void Rotate(float AngleX, float AngleY, float AngleZ);

	std::string Name;
	bool		Dirty;

	uint32_t				MeshIndex		= -1;
	uint32_t				MaterialIndex	= -1;
	Transform				Transform, PreviousTransform;
	DirectX::BoundingBox	BoundingBox;
	uint32_t				InstanceID		= -1;
};