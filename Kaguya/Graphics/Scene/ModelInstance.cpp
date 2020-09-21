#include "pch.h"
#include "ModelInstance.h"

ModelInstance::ModelInstance(const Model* pModel, const Material* pMaterial)
	: pModel(pModel),
	pMaterial(pMaterial)
{
	BoundingBox = pModel->BoundingBox;

	MeshInstances.reserve(pModel->Meshes.size());
	for (const auto& mesh : pModel->Meshes)
	{
		MeshInstance meshInstance = {};
		meshInstance.pMesh = &mesh;
		meshInstance.Transform = Transform;
		meshInstance.BoundingBox = mesh.BoundingBox;

		MeshInstances.push_back(meshInstance);
	}
}

void ModelInstance::SetTransform(DirectX::FXMMATRIX M)
{
	Transform.SetTransform(M);
	for (auto& meshInstance : MeshInstances)
	{
		meshInstance.Transform.SetTransform(M);
	}
}

void ModelInstance::Translate(float DeltaX, float DeltaY, float DeltaZ)
{
	Transform.Translate(DeltaX, DeltaY, DeltaZ);
	for (auto& meshInstance : MeshInstances)
	{
		meshInstance.Transform.Translate(DeltaX, DeltaY, DeltaZ);
	}
}

void ModelInstance::Rotate(float AngleX, float AngleY, float AngleZ)
{
	Transform.Rotate(AngleX, AngleY, AngleZ);
	for (auto& meshInstance : MeshInstances)
	{
		meshInstance.Transform.Rotate(AngleX, AngleY, AngleZ);
	}
}