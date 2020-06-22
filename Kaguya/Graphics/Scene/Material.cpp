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
	: pMaps{ nullptr }
{
	Flags = 0;
}

Material::Material(const Material& rhs)
{
	*this = rhs;
}

Material::Material(Material&& rhs) noexcept
{
	*this = std::move(rhs);
}

Material& Material::operator=(const Material& rhs)
{
	if (this != &rhs)
	{
		Name = rhs.Name;
		Properties = rhs.Properties;
		for (int i = 0; i < NumTextureTypes; ++i)
		{
			Maps[i] = rhs.Maps[i];
			pMaps[i] = rhs.pMaps[i];
		}
	}
	return *this;
}

Material& Material::operator=(Material&& rhs) noexcept
{
	Name = std::move(rhs.Name);
	Properties = std::move(rhs.Properties);
	for (int i = 0; i < NumTextureTypes; ++i)
	{
		Maps[i] = std::move(rhs.Maps[i]);
		pMaps[i] = std::move(rhs.pMaps[i]);
		ScratchImages[i] = std::move(rhs.ScratchImages[i]);
	}
	return *this;
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