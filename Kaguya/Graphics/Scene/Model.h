#pragma once
#include "Vertex.h"
#include "Mesh.h"
#include "Material.h"

struct EmbeddedTexture
{
	std::uint32_t Width;
	std::uint32_t Height;
	bool IsCompressed;
	std::string Extension;
	std::vector<unsigned char> Data;
};

struct Model
{
	std::string Path;
	Transform Transform;
	DirectX::BoundingBox BoundingBox;
	std::vector<Vertex> Vertices;
	std::vector<std::uint32_t> Indices;

	std::vector<Mesh> Meshes;
	std::vector<Material> Materials;
	std::vector<EmbeddedTexture> EmbeddedTextures;

	void SetTransform(DirectX::FXMMATRIX M);
	void Translate(float DeltaX, float DeltaY, float DeltaZ);
	void Rotate(float AngleX, float AngleY, float AngleZ);
};

Model CreateBox(float Width, float Height, float Depth, UINT NumSubdivisions, Material Material);
Model CreateGrid(float Width, float Depth, UINT M, UINT N, Material Material);