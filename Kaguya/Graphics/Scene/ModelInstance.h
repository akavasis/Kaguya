#pragma once
#include "Model.h"
#include "Material.h"
#include "MeshInstance.h"

class ModelInstance
{
public:
	ModelInstance(const Model* pModel, const Material* pMaterial);

	const Model* pModel;
	const Material* pMaterial;

	Transform Transform, PreviousTransform;
	DirectX::BoundingBox BoundingBox;
	std::vector<MeshInstance> MeshInstances;

	void Translate(float DeltaX, float DeltaY, float DeltaZ);
	void SetScale(float Scale);
	void SetScale(float ScaleX, float ScaleY, float ScaleZ);
	void Rotate(float AngleX, float AngleY, float AngleZ);
};