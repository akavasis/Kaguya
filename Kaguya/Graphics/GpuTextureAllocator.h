#pragma once
#include <d3d12.h>
#include <vector>
#include <unordered_set>
#include <unordered_map>

#include <Core/Allocator/VariableSizedAllocator.h>

#include "RenderDevice.h"
#include "RenderContext.h"
#include "Scene/Scene.h"

class GpuTextureAllocator
{
public:
	enum SystemReservedTextures
	{
		BRDFLUT,
		LTCLUT1,
		LTCLUT2,
		SkyboxEquirectangularMap,
		SkyboxCubemap,
		NumSystemReservedTextures
	};
	RenderResourceHandle SystemReservedTextures[NumSystemReservedTextures];

	std::unordered_map<std::string, RenderResourceHandle> TextureHandles;

	GpuTextureAllocator(RenderDevice* pRenderDevice);

	void Stage(Scene& Scene, RenderContext& RenderContext);
	void DisposeResources();
private:
	struct Status
	{
		inline static bool BRDFGenerated = false;
		inline static bool LTCLUT1Generated = false;
		inline static bool LTCLUT2Generated = false;
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
		bool											IsMasked;					// indicates whether or not alpha is masked
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
	bool IsSRGB(DXGI_FORMAT Format);
	DXGI_FORMAT GetUAVCompatableFormat(DXGI_FORMAT Format);

	RenderDevice* pRenderDevice;

	std::unordered_set<DXGI_FORMAT> m_UAVSupportedFormat;

	std::unordered_map<RenderResourceHandle, StagingTexture> m_UnstagedTextures;
	std::vector<RenderResourceHandle> m_TemporaryResources;
};