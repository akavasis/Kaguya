#ifndef __SHARED_TYPES_HLSLI__
#define __SHARED_TYPES_HLSLI__
#include "SharedDefines.hlsli"
#include "Light.hlsli"
#include "Material.hlsli"

struct ObjectConstants
{
	matrix World;
	uint MaterialIndex;
	float3 _padding;
};

struct GlobalConstants
{
	float3 CameraU;
	float _padding;
	
	float3 CameraV;
	float _padding1;
	
	float3 CameraW;
	float _padding2;
	
	matrix View;
	matrix Projection;
	matrix InvView;
	matrix InvProjection;
	matrix ViewProjection;
	float3 EyePosition;
	uint TotalFrameCount;

	DirectionalLight Sun;
	
	uint MaxDepth;
	float FocalLength;
	float LensRadius;
};

struct GeometryInfo
{
	uint VertexOffset;
	uint IndexOffset;
	uint MaterialIndex;
	matrix World;
};
#endif