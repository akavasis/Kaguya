#pragma once
#include <d3d12.h>
#include <vector>
#include <unordered_set>
#include <unordered_map>

#include <DirectXTex.h>

#include "RenderDevice.h"
#include "Scene/Scene.h"

class GpuTextureAllocator
{
public:
	GpuTextureAllocator(RenderDevice& RefRenderDevice);

	void Stage(Scene& Scene, RenderCommandContext* pRenderCommandContext);
	void Bind(RenderCommandContext* pRenderCommandContext) const;
private:
	struct Status
	{
		bool BRDFGenerated = false;

	} m_Status;

	struct StagingTexture
	{
		std::string path;															// file path
		Texture texture;															// gpu upload buffer
		std::size_t numSubresources;												// number of subresources
		std::vector<D3D12_PLACED_SUBRESOURCE_FOOTPRINT> placedSubresourceLayouts;	// footprint is synonymous to layout
		std::size_t mipLevels;														// indicates what mip levels this texture should have
		bool generateMips;															// indicates whether or not we should generate mips for the staged texture
	};

	RenderResourceHandle LoadFromFile(const std::filesystem::path& Path, bool ForceSRGB, bool GenerateMips);
	void LoadMaterial(Material& Material);
	void GenerateMips(RenderResourceHandle TextureHandle, RenderCommandContext* pRenderCommandContext);
	void GenerateMipsUAV(RenderResourceHandle TextureHandle, RenderCommandContext* pRenderCommandContext);
	void GenerateMipsSRGB(RenderResourceHandle TextureHandle, RenderCommandContext* pRenderCommandContext);
	void EquirectangularToCubemap(RenderResourceHandle EquirectangularMap, RenderResourceHandle Cubemap, RenderCommandContext* pRenderCommandContext);
	void EquirectangularToCubemapUAV(RenderResourceHandle EquirectangularMap, RenderResourceHandle Cubemap, RenderCommandContext* pRenderCommandContext);
	void EquirectangularToCubemapSRGB(RenderResourceHandle EquirectangularMap, RenderResourceHandle Cubemap, RenderCommandContext* pRenderCommandContext);

	bool IsUAVCompatable(DXGI_FORMAT Format);
	bool IsSRGB(DXGI_FORMAT Format);
	DXGI_FORMAT GetUAVCompatableFormat(DXGI_FORMAT Format);

	RenderDevice& m_RefRenderDevice;

	DescriptorAllocator m_TextureViewAllocator;
	Descriptor m_MipsNullUAVs;
	Descriptor m_EquirectangularToCubeMapNullUAVs;
	Descriptor m_BRDFLUTRTV;
	Descriptor m_SkyboxSRVs;

	std::unordered_set<DXGI_FORMAT> m_UAVSupportedFormat;

	// TODO: Add method to free them after Stage call
	std::unordered_map<RenderResourceHandle, StagingTexture> m_UnstagedTextures;
	std::vector<RenderResourceHandle> m_TemporaryResources;
	std::vector<Descriptor> m_TemporaryDescriptors;

	RenderResourceHandle m_CubemapCamerasHandle;
	RenderResourceHandle m_BRDFLUTHandle;
	RenderResourceHandle m_SkyboxEquirectangularHandle;
	RenderResourceHandle m_SkyboxCubemapHandle;
	RenderResourceHandle m_IrradianceHandle;
	RenderResourceHandle m_PrefilteredHandle;
	std::unordered_map<std::string, RenderResourceHandle> m_TextureHandles;
};