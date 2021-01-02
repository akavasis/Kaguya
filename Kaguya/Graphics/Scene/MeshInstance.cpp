#include "pch.h"
#include "MeshInstance.h"

MeshInstance::MeshInstance(size_t MeshIndex, size_t MaterialIndex)
	: MeshIndex(MeshIndex),
	MaterialIndex(MaterialIndex)
{

}

void MeshInstance::Translate(float DeltaX, float DeltaY, float DeltaZ)
{
	PreviousTransform = Transform;
	Transform.Translate(DeltaX, DeltaY, DeltaZ);
}

void MeshInstance::SetScale(float Scale)
{
	PreviousTransform = Transform;
	Transform.SetScale(Scale, Scale, Scale);
}

void MeshInstance::SetScale(float ScaleX, float ScaleY, float ScaleZ)
{
	PreviousTransform = Transform;
	Transform.SetScale(ScaleX, ScaleY, ScaleZ);
}

void MeshInstance::Rotate(float AngleX, float AngleY, float AngleZ)
{
	PreviousTransform = Transform;
	Transform.Rotate(AngleX, AngleY, AngleZ);
}