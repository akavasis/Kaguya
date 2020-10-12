#ifndef __SHARED_TYPES_H__
#define __SHARED_TYPES_H__
#include "SharedDefines.h"
#include "Light.h"
#include "Material.h"

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

	int NumSamplesPerPixel;
	int MaxDepth;
	float FocalLength;
	float LensRadius;

	int SkyboxIndex;
};

struct GeometryInfo
{
	uint VertexOffset;
	uint IndexOffset;
	uint MaterialIndex;
	matrix World;
};
#endif