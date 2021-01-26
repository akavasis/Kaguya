#include "pch.h"
#include "Material.h"

Material::Material()
{
	Albedo					= { 0.0f, 0.0f, 0.0f };
	Emissive				= { 0.0f, 0.0f, 0.0f };
	Specular				= { 0.0f, 0.0f, 0.0f };
	Refraction				= { 0.0f, 0.0f, 0.0f };
	SpecularChance			= 0.0f;
	Roughness				= 1.0f;
	Metallic				= 0.0f;
	Fuzziness				= 0.0f;
	IndexOfRefraction		= 1.0f;
	Model					= 0;
	UseAttributes			= false;

	for (int i = 0; i < NumTextureTypes; ++i)
	{
		TextureIndices[i]	= 0;
		TextureChannel[i]	= 0;
		Textures[i]			= {};
	}
}