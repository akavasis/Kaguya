#pragma once
#include <memory>
#include <functional>

#include "../RenderDevice.h"
#include "../Vertex.h"

namespace Asset
{
	struct Mesh;

	struct MeshMetadata
	{
		std::filesystem::path Path;
		bool KeepGeometryInRAM;
	};

	struct Mesh
	{
		MeshMetadata Metadata;

		std::string						Name;
		DirectX::BoundingBox			BoundingBox;

		uint32_t						VertexCount = 0;
		uint32_t						IndexCount = 0;

		std::vector<Vertex>				Vertices;
		std::vector<uint32_t>			Indices;

		std::shared_ptr<Resource> VertexResource;
		std::shared_ptr<Resource> IndexResource;
		std::shared_ptr<Resource> AccelerationStructure;
		BottomLevelAccelerationStructure BLAS;
	};
}

Asset::Mesh CreateBox(float Width, float Height, float Depth);
Asset::Mesh CreateGrid(float Width, float Depth, uint32_t M, uint32_t N);