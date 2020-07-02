#include "pch.h"
#include "Material.h"

Material::Properties::Properties()
{
	Albedo = DirectX::XMFLOAT3(1.0f, 1.0f, 1.0f);
	Metallic = 0.1f;
	Emissive = DirectX::XMFLOAT3(0.0f, 0.0f, 0.0f);
	Roughness = 0.5f;
}

Material::Material()
{
	Flags = 0;
}

Material::~Material()
{
}

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