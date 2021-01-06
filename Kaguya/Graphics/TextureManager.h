#pragma once

#include <d3d12.h>
#include <vector>
#include <unordered_map>

#include <DirectXTex.h>

#include "RenderDevice.h"
#include "RenderContext.h"
#include "Scene/Scene.h"

class TextureManager
{
public:
	TextureManager(RenderDevice* pRenderDevice);

	inline auto GetDefaultWhiteTexture()						{ return m_SystemTextures[AssetTextures::DefaultWhite]; }
	inline auto GetDefaultBlackTexture()						{ return m_SystemTextures[AssetTextures::DefaultBlack]; }
	inline auto GetDefaultAlbedoTexture()						{ return m_SystemTextures[AssetTextures::DefaultAlbedo]; }
	inline auto GetDefaultNormalTexture()						{ return m_SystemTextures[AssetTextures::DefaultNormal]; }
	inline auto GetDefaultRoughnessTexture()					{ return m_SystemTextures[AssetTextures::DefaultRoughness]; }
	inline auto GetLTC_LUT_DisneyDiffuse_InverseMatrixTexture() { return m_SystemTextures[AssetTextures::LTC_DisneyDiffuse_InverseMatrix]; }
	inline auto GetLTC_LUT_DisneyDiffuse_TermsTexture()			{ return m_SystemTextures[AssetTextures::LTC_DisneyDiffuse_Terms]; }
	inline auto GetLTC_LUT_GGX_InverseMatrixTexture()			{ return m_SystemTextures[AssetTextures::LTC_GGX_InverseMatrix]; }
	inline auto GetLTC_LUT_GGX_TermsTexture()					{ return m_SystemTextures[AssetTextures::LTC_GGX_Terms]; }

	inline auto GetBlueNoise()									{ return m_NoiseTextures[AssetTextures::BlueNoise]; }

	inline auto GetSkybox()										{ return m_Skybox; }

	void Stage(Scene& Scene, RenderContext& RenderContext);
	void DisposeResources();

private:
	struct AssetTextures
	{
		// These textures can be created using small resources
		enum System
		{
			DefaultWhite,
			DefaultBlack,
			DefaultAlbedo,
			DefaultNormal,
			DefaultRoughness,
			LTC_DisneyDiffuse_InverseMatrix,
			LTC_DisneyDiffuse_Terms,
			LTC_GGX_InverseMatrix,
			LTC_GGX_Terms,
			NumSystemTextures
		};

		enum Noise
		{
			BlueNoise,
			NumNoiseTextures
		};
	};

	struct Status
	{
		inline static bool SkyboxGenerated = false;
	};

	struct StagingTexture
	{
		std::string										Name;						// File path
		Texture											Texture;					// Gpu upload buffer
		std::size_t										NumSubresources;			// Number of subresources
		std::vector<D3D12_PLACED_SUBRESOURCE_FOOTPRINT> PlacedSubresourceLayouts;	// Footprint is synonymous to layout
		std::size_t										MipLevels;					// Indicates what mip levels this texture should have
		bool											GenerateMips;				// Indicates whether or not we should generate mips for the staged texture
	};

	StagingTexture CreateStagingTexture(std::string Name, D3D12_RESOURCE_DESC Desc, const DirectX::ScratchImage& ScratchImage, std::size_t MipLevels, bool GenerateMips);

	void LoadSystemTextures();
	void LoadNoiseTextures();
	RenderResourceHandle LoadFromFile(const std::filesystem::path& Path, bool ForceSRGB, bool GenerateMips);
	void LoadMaterial(Material& Material);
	void StageTexture(RenderResourceHandle TextureHandle, StagingTexture& StagingTexture, RenderContext& RenderContext);
	void GenerateMipsUAV(RenderResourceHandle TextureHandle, RenderContext& RenderContext);
	void GenerateMipsSRGB(const std::string& Name, RenderResourceHandle TextureHandle, RenderContext& RenderContext);
	void EquirectangularToCubemap(const std::string& Name, RenderResourceHandle EquirectangularMap, RenderResourceHandle Cubemap, RenderContext& RenderContext);
	void EquirectangularToCubemapUAV(RenderResourceHandle EquirectangularMap, RenderResourceHandle Cubemap, RenderContext& RenderContext);
	void EquirectangularToCubemapSRGB(const std::string& Name, RenderResourceHandle EquirectangularMap, RenderResourceHandle Cubemap, RenderContext& RenderContext);

	DXGI_FORMAT GetUAVCompatableFormat(DXGI_FORMAT Format);

	RenderDevice*												pRenderDevice;

	RenderResourceHandle										m_SystemTextureHeap;
	RenderResourceHandle										m_SystemTextures[AssetTextures::NumSystemTextures];

	RenderResourceHandle										m_NoiseTextureHeap;
	RenderResourceHandle										m_NoiseTextures[AssetTextures::NumNoiseTextures];

	RenderResourceHandle										m_SkyboxEquirectangular;
	RenderResourceHandle										m_Skybox;
	std::unordered_map<std::string, RenderResourceHandle>		m_TextureCache;

	std::unordered_map<RenderResourceHandle, StagingTexture>	m_UnstagedTextures;
	std::vector<RenderResourceHandle>							m_TemporaryResources;
};