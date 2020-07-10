#pragma once
#include <string>

#include <DirectXMath.h>

#include "../RenderResourceHandle.h"

struct Material
{
	enum Type
	{
		Albedo,
		Normal,
		Roughness,
		Metallic,
		Emissive,
		NumTextureTypes
	};

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
		DirectX::XMFLOAT3 Albedo = { 1.0f, 1.0f, 1.0f };
		float Metallic = 0.1f;
		DirectX::XMFLOAT3 Emissive { 0.0f, 0.0f, 0.0f };
		float Roughness = 0.5f;
	};

	Properties Properties;
	std::string Paths[NumTextureTypes];
	RenderResourceHandle Textures[NumTextureTypes];
	int Flags = 0;

	bool HaveAlbedoMap() const;
	bool HaveNormalMap() const;
	bool HaveRoughnessMap() const;
	bool HaveMetallicMap() const;
	bool HaveEmissiveMap() const;
};