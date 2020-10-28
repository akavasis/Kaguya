#pragma once
#include <d3d12.h>
#include <vector>
#include <unordered_set>
#include <unordered_map>

#include "RenderDevice.h"
#include "RenderContext.h"
#include "Scene/Scene.h"

class GpuTextureAllocator
{
public:
	GpuTextureAllocator(RenderDevice* SV_pRenderDevice);

	inline auto GetDefaultWhiteTexture()				{ return m_SystemReservedTextures[DefaultWhite]; }
	inline auto GetDefaultBlackTexture()				{ return m_SystemReservedTextures[DefaultBlack]; }
	inline auto GetDefaultAlbedoTexture()				{ return m_SystemReservedTextures[DefaultAlbedo]; }
	inline auto GetDefaultNormalTexture()				{ return m_SystemReservedTextures[DefaultNormal]; }
	inline auto GetDefaultRoughnessTexture()			{ return m_SystemReservedTextures[DefaultRoughness]; }
	inline auto GetBRDFLUTTexture()						{ return m_SystemReservedTextures[BRDFLUT]; }
	inline auto GetLTC_LUT_GGX_InverseMatrixTexture()	{ return m_SystemReservedTextures[LTC_LUT_GGX_InverseMatrix]; }
	inline auto GetLTC_LUT_GGX_TermsTexture()			{ return m_SystemReservedTextures[LTC_LUT_GGX_Terms]; }
	inline auto GetSkyboxTexture()						{ return m_SystemReservedTextures[SkyboxCubemap]; }

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
		LTC_LUT_GGX_InverseMatrix,
		LTC_LUT_GGX_Terms,
		SkyboxEquirectangularMap,
		SkyboxCubemap,
		NumSystemReservedTextures
	};

	struct Status
	{
		inline static bool SkyboxGenerated = false;
	};

	struct StagingTexture
	{
		std::string										Path;						// file path
		DeviceTexture									Texture;					// gpu upload buffer
		std::size_t										NumSubresources;			// number of subresources
		std::vector<D3D12_PLACED_SUBRESOURCE_FOOTPRINT> PlacedSubresourceLayouts;	// footprint is synonymous to layout
		std::size_t										MipLevels;					// indicates what mip levels this texture should have
		bool											GenerateMips;				// indicates whether or not we should generate mips for the staged texture
	};

	RenderResourceHandle LoadFromFile(const std::filesystem::path& Path, bool ForceSRGB, bool GenerateMips);
	void LoadMaterial(Material& Material);
	void StageTexture(RenderResourceHandle TextureHandle, StagingTexture& StagingTexture, RenderContext& RenderContext);
	void GenerateMipsUAV(RenderResourceHandle TextureHandle, RenderContext& RenderContext);
	void GenerateMipsSRGB(RenderResourceHandle TextureHandle, RenderContext& RenderContext);
	void EquirectangularToCubemap(RenderResourceHandle EquirectangularMap, RenderResourceHandle Cubemap, RenderContext& RenderContext);
	void EquirectangularToCubemapUAV(RenderResourceHandle EquirectangularMap, RenderResourceHandle Cubemap, RenderContext& RenderContext);
	void EquirectangularToCubemapSRGB(RenderResourceHandle EquirectangularMap, RenderResourceHandle Cubemap, RenderContext& RenderContext);

	bool IsUAVCompatable(DXGI_FORMAT Format);
	DXGI_FORMAT GetUAVCompatableFormat(DXGI_FORMAT Format);

	RenderDevice*												SV_pRenderDevice;

	RenderResourceHandle										m_SystemReservedTextures[NumSystemReservedTextures];
	std::unordered_map<std::string, RenderResourceHandle>		m_Textures;

	std::unordered_set<DXGI_FORMAT>								m_UAVSupportedFormat;
	std::unordered_map<RenderResourceHandle, StagingTexture>	m_UnstagedTextures;
	std::vector<RenderResourceHandle>							m_TemporaryResources;
};