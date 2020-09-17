#ifndef __SHARED_TYPES_HLSLI__
#define __SHARED_TYPES_HLSLI__
#include "SharedDefines.hlsli"
#include "Light.hlsli"

#define MATERIAL_MODEL_LAMBERTIAN 0
#define MATERIAL_MODEL_METAL 1
#define MATERIAL_MODEL_DIELECTRIC 2
#define MATERIAL_MODEL_DIFFUSE_LIGHT 3

struct ObjectConstants
{
	matrix World;
	uint MaterialIndex;
	float3 _padding;
};

struct RenderPassConstants
{
	matrix View;
	matrix Projection;
	matrix InvView;
	matrix InvProjection;
	matrix ViewProjection;
	float3 EyePosition;
	uint TotalFrameCount;

	DirectionalLight Sun;
	uint SunShadowMapIndex;
	uint BRDFLUTMapIndex;
	uint RadianceCubemapIndex;
	uint IrradianceCubemapIndex;
	uint PrefilteredRadianceCubemapIndex;
	
	uint MaxDepth;
};

struct MaterialTextureProperties
{
	// Note: diffuse chance is 1.0f - (specularChance+refractionChance)
	float3	Albedo;					// the color used for diffuse lighting
	float3	Emissive;				// how much the surface glows
	float	SpecularChance;			// percentage chance of doing a specular reflection
	float	SpecularRoughness;		// how rough the specular reflections are
	float3	SpecularColor;			// the color tint of specular reflections
	float	IndexOfRefraction;		// index of refraction. used by fresnel and refraction.
	float	RefractionChance;		// percent chance of doing a refractive transmission
	float	RefractionRoughness;	// how rough the refractive transmissions are
	float3	RefractionColor;		// absorption for beer's law
	uint	Model;					// describes the material model
};

struct MaterialTextureIndices
{
	int AlbedoMapIndex;
	int NormalMapIndex;
	int RoughnessMapIndex;
	int MetallicMapIndex;
	int EmissiveMapIndex;
	bool IsMasked;
};

struct GeometryInfo
{
	uint VertexOffset;
	uint IndexOffset;
	uint MaterialIndex;
	matrix World;
};
#endif