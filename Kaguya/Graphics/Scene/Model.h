#pragma once
#include "Vertex.h"
#include "Mesh.h"

struct Model
{
	std::string					Path;
	DirectX::BoundingBox		BoundingBox;
	std::vector<Vertex>			Vertices;
	std::vector<uint32_t>		Indices;

	std::vector<Mesh>			Meshes;
};

Model CreateTriangle();
Model CreateBox(float Width, float Height, float Depth, UINT NumSubdivisions);
Model CreateGrid(float Width, float Depth, UINT M, UINT N);