#include "pch.h"
#include "Model.h"

void Model::SetTransform(DirectX::FXMMATRIX M)
{
	for (auto& mesh : Meshes)
	{
		mesh.Transform.SetTransform(M);
	}
}

void Model::Translate(float DeltaX, float DeltaY, float DeltaZ)
{
	for (auto& mesh : Meshes)
	{
		mesh.Transform.Translate(DeltaX, DeltaY, DeltaZ);
	}
}

void Model::Rotate(float AngleX, float AngleY, float AngleZ)
{
	for (auto& mesh : Meshes)
	{
		mesh.Transform.Rotate(AngleX, AngleY, AngleZ);
	}
}