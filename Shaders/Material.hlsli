#ifndef MATERIAL_HLSLI
#define MATERIAL_HLSLI

enum MaterialModel
{
	LambertianModel,
	GlossyModel,
	MetalModel,
	DielectricModel,
	DiffuseLightModel
};

enum TextureTypes
{
	AlbedoIdx,
	NormalIdx,
	RoughnessIdx,
	MetallicIdx,
	EmissiveIdx,
	NumTextureTypes
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
};

#endif // MATERIAL_HLSLI