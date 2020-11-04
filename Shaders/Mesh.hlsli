#ifndef MESH_HLSLI
#define MESH_HLSLI

struct Mesh
{
	uint	VertexOffset;
	uint	IndexOffset;
	uint	MaterialIndex;
	matrix	World;
	matrix	PreviousWorld;
};

#endif