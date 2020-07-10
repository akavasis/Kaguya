#include "pch.h"
#include "Material.h"

bool Material::HaveAlbedoMap() const
{
	return (Flags & MATERIAL_FLAG_HAVE_ALBEDO_TEXTURE) > 0;
}

bool Material::HaveNormalMap() const
{
	return (Flags & MATERIAL_FLAG_HAVE_NORMAL_TEXTURE) > 0;
}

bool Material::HaveRoughnessMap() const
{
	return (Flags & MATERIAL_FLAG_HAVE_ROUGHNESS_TEXTURE) > 0;
}

bool Material::HaveMetallicMap() const
{
	return (Flags & MATERIAL_FLAG_HAVE_METALLIC_TEXTURE) > 0;
}

bool Material::HaveEmissiveMap() const
{
	return (Flags & MATERIAL_FLAG_HAVE_EMISSIVE_TEXTURE) > 0;
}