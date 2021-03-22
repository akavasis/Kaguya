#include "pch.h"
#include "Material.h"

Material::Material()
{
	for (int i = 0; i < NumTextureTypes; ++i)
	{
		TextureIndices[i]	= 0;
		TextureChannel[i]	= 0;
		Textures[i]			= {};
	}
}