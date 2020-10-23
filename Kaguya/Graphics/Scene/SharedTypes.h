#pragma once
#include "SharedDefines.h"
#include "Light.h"
#include "Material.h"

struct HostSystemConstants
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

	int NumSamplesPerPixel;
	int MaxDepth;
	float FocalLength;
	float LensRadius;

	int SkyboxIndex;
	int NumPolygonalLights;
};

struct GeometryInfo
{
	uint VertexOffset;
	uint IndexOffset;
	uint MaterialIndex;
	matrix World;
};