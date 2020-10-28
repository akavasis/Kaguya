#pragma once
#include "SharedDefines.h"

namespace HLSL
{
	struct PolygonalLight
	{
		matrix	World;
		float3	Color;
		float	Luminance;
		float	Width;
		float	Height;
	};

	struct Material
	{
		float3	Albedo;
		float3	Emissive;
		float3	Specular;
		float3	Refraction;
		float	SpecularChance;
		float	Roughness;
		float	Metallic;
		float	Fuzziness;
		float	IndexOfRefraction;
		uint	Model;

		int		TextureIndices[NumTextureTypes];
	};

	struct Camera
	{
		float	NearZ;
		float	FarZ;
		float	ExposureValue100;
		float	_padding1;

		float	FocalLength;
		float	RelativeAperture;
		float	ShutterTime;
		float	SensorSensitivity;

		float4	Position;
		float4	U;
		float4	V;
		float4	W;

		matrix	View;
		matrix	Projection;
		matrix	ViewProjection;
		matrix	InvView;
		matrix	InvProjection;
	};

	struct Mesh
	{
		uint	VertexOffset;
		uint	IndexOffset;
		uint	MaterialIndex;
		matrix	World;
	};

	struct SystemConstants
	{
		Camera Camera;

		uint TotalFrameCount;

		int NumPolygonalLights;
	};
}