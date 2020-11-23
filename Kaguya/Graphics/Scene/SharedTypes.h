#pragma once
#include "SharedDefines.h"

namespace HLSL
{
	struct PolygonalLight
	{
		float3	Position;
		float4	Orientation;
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
		uint	UseAttributeAsValues; // If this is true, then the attributes above will be used rather than actual textures

		int		TextureIndices[NumTextureTypes];
		int		TextureChannel[NumTextureTypes];
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
		matrix	InvViewProjection;
	};

	struct Mesh
	{
		uint	VertexOffset;
		uint	IndexOffset;
		uint	MaterialIndex;
		matrix	World;
		matrix	PreviousWorld;
	};

	struct SystemConstants
	{
		Camera Camera, PreviousCamera;

		float4 OutputSize;

		uint TotalFrameCount;

		int NumPolygonalLights;
	};
}