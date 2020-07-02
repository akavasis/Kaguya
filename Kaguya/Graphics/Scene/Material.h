#pragma once
#include <string>

#include <DirectXMath.h>

#include "../RenderResourceHandle.h"

enum TextureType
{
	Albedo,
	Normal,
	Roughness,
	Metallic,
	Emissive,
	NumTextureTypes
};

struct Material
{
	enum Flags : int
	{
		MATERIAL_FLAG_HAVE_ALBEDO_TEXTURE = 0x1,
		MATERIAL_FLAG_HAVE_NORMAL_TEXTURE = 0x2,
		MATERIAL_FLAG_HAVE_ROUGHNESS_TEXTURE = 0x4,
		MATERIAL_FLAG_HAVE_METALLIC_TEXTURE = 0x8,
		MATERIAL_FLAG_HAVE_EMISSIVE_TEXTURE = 0x10,
		MATERIAL_FLAG_IS_MASKED = 0x20,
	};

	struct Properties
	{
		DirectX::XMFLOAT3 Albedo;
		float Metallic;
		DirectX::XMFLOAT3 Emissive;
		float Roughness;

		Properties();
	};

	Material();
	~Material();

	bool HaveAlbedoMap() const;
	bool HaveNormalMap() const;
	bool HaveRoughnessMap() const;
	bool HaveMetallicMap() const;
	bool HaveEmissiveMap() const;

	std::string Name;
	Properties Properties;
	std::string Maps[NumTextureTypes];
	union
	{
		struct
		{
			RenderResourceHandle Albedo;
			RenderResourceHandle Normal;
			RenderResourceHandle Roughness;
			RenderResourceHandle Metallic;
			RenderResourceHandle Emissive;
		};
		RenderResourceHandle Textures[NumTextureTypes];
	};
	int Flags;
};