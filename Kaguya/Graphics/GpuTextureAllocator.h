#pragma once
#include <d3d12.h>
#include <vector>
#include <unordered_set>
#include <unordered_map>

#include <DirectXTex.h>

#include "RenderDevice.h"
#include "../Core/Allocator/VariableSizedAllocator.h"
#include "Scene/Scene.h"

class GpuTextureAllocator
{
public:
	//enum RendererReseveredTextures
	//{
	//	BRDFLUT,
	//	SkyboxEquirectangularMap,
	//	SkyboxCubemap,
	//	SkyboxIrradianceCubemap,
	//	SkyboxPrefilteredCubemap,
	//	NumAssetTextures
	//};
	//RenderResourceHandle RendererReseveredTextures[NumAssetTextures];

	std::unordered_map<std::string, RenderResourceHandle> TextureHandles;

	GpuTextureAllocator(RenderDevice* pRenderDevice);

	void Stage(Scene& Scene, CommandContext* pCommandContext);
private:
	struct Status
	{
		inline static bool BRDFGenerated = false;
	};

	struct StagingTexture
	{
		std::string path;															// file path
		Texture texture;															// gpu upload buffer
		std::size_t numSubresources;												// number of subresources
		std::vector<D3D12_PLACED_SUBRESOURCE_FOOTPRINT> placedSubresourceLayouts;	// footprint is synonymous to layout
		std::size_t mipLevels;														// indicates what mip levels this texture should have
		bool generateMips;															// indicates whether or not we should generate mips for the staged texture
		bool isMasked;																// indicates whether or not alpha is masked
	};

	RenderResourceHandle LoadFromFile(const std::filesystem::path& Path, bool ForceSRGB, bool GenerateMips);
	void LoadMaterial(Material& Material);
	void StageTexture(RenderResourceHandle TextureHandle, StagingTexture& StagingTexture, CommandContext* pCommandContext);
	void GenerateMips(RenderResourceHandle TextureHandle, CommandContext* pCommandContext);
	void GenerateMipsUAV(RenderResourceHandle TextureHandle, CommandContext* pCommandContext);
	void GenerateMipsSRGB(RenderResourceHandle TextureHandle, CommandContext* pCommandContext);
	void EquirectangularToCubemap(RenderResourceHandle EquirectangularMap, RenderResourceHandle Cubemap, CommandContext* pCommandContext);
	void EquirectangularToCubemapUAV(RenderResourceHandle EquirectangularMap, RenderResourceHandle Cubemap, CommandContext* pCommandContext);
	void EquirectangularToCubemapSRGB(RenderResourceHandle EquirectangularMap, RenderResourceHandle Cubemap, CommandContext* pCommandContext);

	bool IsUAVCompatable(DXGI_FORMAT Format);
	bool IsSRGB(DXGI_FORMAT Format);
	DXGI_FORMAT GetUAVCompatableFormat(DXGI_FORMAT Format);

	RenderDevice* pRenderDevice;

	std::unordered_set<DXGI_FORMAT> m_UAVSupportedFormat;

	// TODO: Add method to free them after Stage call
	std::unordered_map<RenderResourceHandle, StagingTexture> m_UnstagedTextures;
	std::vector<RenderResourceHandle> m_TemporaryResources;

	RenderResourceHandle m_CubemapCamerasUploadBufferHandle;
};