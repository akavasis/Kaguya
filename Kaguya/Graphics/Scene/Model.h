#pragma once
#include "Vertex.h"
#include "Mesh.h"
#include "Material.h"

#include "../RenderResourceHandle.h"

struct Model
{
	DirectX::BoundingBox BoundingBox; // The bounding box that encompasses all of the meshes
	std::vector<Mesh> Meshes;
	std::vector<Material> Materials;

	void SetTransform(DirectX::FXMMATRIX M);
	void Translate(float DeltaX, float DeltaY, float DeltaZ);
	void Rotate(float AngleX, float AngleY, float AngleZ);
};