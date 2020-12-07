#pragma once
#include <d3d12.h>
#include <vector>
#include <unordered_map>

#include "RenderDevice.h"
#include "RenderContext.h"
#include "Scene/Scene.h"

class TextureManager
{
public:
	TextureManager(RenderDevice* pRenderDevice);

	inline auto GetDefaultWhiteTexture()						{ return m_SystemReservedTextures[DefaultWhite]; }
	inline auto GetDefaultBlackTexture()						{ return m_SystemReservedTextures[DefaultBlack]; }
	inline auto GetDefaultAlbedoTexture()						{ return m_SystemReservedTextures[DefaultAlbedo]; }
	inline auto GetDefaultNormalTexture()						{ return m_SystemReservedTextures[DefaultNormal]; }
	inline auto GetDefaultRoughnessTexture()					{ return m_SystemReservedTextures[DefaultRoughness]; }
	inline auto GetBRDFLUTTexture()								{ return m_SystemReservedTextures[BRDFLUT]; }
	inline auto GetLTC_LUT_DisneyDiffuse_InverseMatrixTexture() { return m_SystemReservedTextures[LTC_LUT_DisneyDiffuse_InverseMatrix]; }
	inline auto GetLTC_LUT_DisneyDiffuse_TermsTexture()			{ return m_SystemReservedTextures[LTC_LUT_DisneyDiffuse_Terms]; }
	inline auto GetLTC_LUT_GGX_InverseMatrixTexture()			{ return m_SystemReservedTextures[LTC_LUT_GGX_InverseMatrix]; }
	inline auto GetLTC_LUT_GGX_TermsTexture()					{ return m_SystemReservedTextures[LTC_LUT_GGX_Terms]; }
	inline auto GetBlueNoise()									{ return m_SystemReservedTextures[BlueNoise]; }

	void StageSystemReservedTextures(RenderContext& RenderContext);
	void Stage(Scene& Scene, RenderContext& RenderContext);
	void DisposeResources();
private:
	enum SystemReservedTextures
	{
		DefaultWhite,
		DefaultBlack,
		DefaultAlbedo,
		DefaultNormal,
		DefaultRoughness,
		BRDFLUT,
		LTC_LUT_DisneyDiffuse_InverseMatrix,
		LTC_LUT_DisneyDiffuse_Terms,
		LTC_LUT_GGX_InverseMatrix,
		LTC_LUT_GGX_Terms,
		BlueNoise,
		NumSystemReservedTextures
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

	RenderResourceHandle										m_SystemReservedTextures[NumSystemReservedTextures];
	std::unordered_map<std::string, RenderResourceHandle>		m_TextureCache;

	std::unordered_map<RenderResourceHandle, StagingTexture>	m_UnstagedTextures;
	std::vector<RenderResourceHandle>							m_TemporaryResources;
};