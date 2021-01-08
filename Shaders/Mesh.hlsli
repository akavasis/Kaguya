#ifndef MESH_HLSLI
#define MESH_HLSLI

struct Mesh
{
	uint		VertexOffset;
	uint		IndexOffset;
	uint		MaterialIndex;
	uint		InstanceIDAndMask;
	uint		InstanceContributionToHitGroupIndexAndFlags;
	uint64_t	AccelerationStructure;
	matrix		World;
	matrix		PreviousWorld;
	float3x4	Transform;
};

#endif