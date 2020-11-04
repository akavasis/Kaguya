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

void ModelInstance::Translate(float DeltaX, float DeltaY, float DeltaZ)
{
	PreviousTransform = Transform;
	Transform.Translate(DeltaX, DeltaY, DeltaZ);
	for (auto& meshInstance : MeshInstances)
	{
		meshInstance.PreviousTransform = meshInstance.Transform;
		meshInstance.Transform.Translate(DeltaX, DeltaY, DeltaZ);
	}
}

void ModelInstance::SetScale(float Scale)
{
	PreviousTransform = Transform;
	Transform.SetScale(Scale, Scale, Scale);
	for (auto& meshInstance : MeshInstances)
	{
		meshInstance.PreviousTransform = meshInstance.Transform;
		meshInstance.Transform.SetScale(Scale, Scale, Scale);
	}
}

void ModelInstance::SetScale(float ScaleX, float ScaleY, float ScaleZ)
{
	PreviousTransform = Transform;
	Transform.SetScale(ScaleX, ScaleY, ScaleZ);
	for (auto& meshInstance : MeshInstances)
	{
		meshInstance.PreviousTransform = meshInstance.Transform;
		meshInstance.Transform.SetScale(ScaleX, ScaleY, ScaleZ);
	}
}

void ModelInstance::Rotate(float AngleX, float AngleY, float AngleZ)
{
	PreviousTransform = Transform;
	Transform.Rotate(AngleX, AngleY, AngleZ);
	for (auto& meshInstance : MeshInstances)
	{
		meshInstance.PreviousTransform = meshInstance.Transform;
		meshInstance.Transform.Rotate(AngleX, AngleY, AngleZ);
	}
}