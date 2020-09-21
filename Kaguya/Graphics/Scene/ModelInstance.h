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

	Transform Transform;
	DirectX::BoundingBox BoundingBox;
	std::vector<MeshInstance> MeshInstances;

	void SetTransform(DirectX::FXMMATRIX M);
	void Translate(float DeltaX, float DeltaY, float DeltaZ);
	void Rotate(float AngleX, float AngleY, float AngleZ);
};