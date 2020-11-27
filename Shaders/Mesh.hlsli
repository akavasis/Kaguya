#ifndef MESH_HLSLI
#define MESH_HLSLI

struct Mesh
{
	uint	VertexOffset;
	uint	IndexOffset;
	uint	MeshletOffset;
	uint 	UniqueVertexIndexOffset;
	uint 	PrimitiveIndexOffset;
	uint	MaterialIndex;
	matrix	World;
	matrix	PreviousWorld;
};

#endif