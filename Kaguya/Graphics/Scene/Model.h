#pragma once
#include "Vertex.h"
#include "Mesh.h"

#include "../RenderResourceHandle.h"

struct Model
{
	std::string					Path;
	DirectX::BoundingBox		BoundingBox;
	std::vector<Vertex>			Vertices;
	std::vector<uint32_t>		Indices;

	std::vector<Mesh>			Meshes;

	RenderResourceHandle		MeshletResource, UploadMeshletResource;
	RenderResourceHandle		UniqueVertexIndexResource, UploadUniqueVertexIndexResource;
	RenderResourceHandle		PrimitiveIndexResource, UploadPrimitiveIndexResource;

	// Iterator interface
	auto begin() { return Meshes.begin(); }
	auto end() { return Meshes.end(); }
};

Model CreateBox(float Width, float Height, float Depth, uint32_t NumSubdivisions);
Model CreateGrid(float Width, float Depth, uint32_t M, uint32_t N);