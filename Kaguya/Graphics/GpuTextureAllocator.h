#pragma once
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

	void Stage(Scene* pScene, RenderCommandContext* pRenderCommandContext);
	void Bind(RenderCommandContext* pRenderCommandContext) const;
private:
	struct StagingTexture
	{
		std::string Path;
		Texture texture;					// Gpu upload buffer
		DirectX::ScratchImage scratchImage;	// image that contains the subresource data to be upload in the texture field
		DirectX::TexMetadata metadata;		// metadata of the scratchImage
		std::size_t mipLevels;				// indicates what mip levels this texture should have
		bool generateMips;					// indicates whether or not we should generate mips for the staged texture
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
	Descriptor m_MipsNullUAV;
	Descriptor m_EquirectangularToCubeMapNullUAV;

	std::unordered_set<DXGI_FORMAT> m_UAVSupportedFormat;

	std::unordered_map<RenderResourceHandle, StagingTexture> m_UnstagedTextures;
	std::vector<RenderResourceHandle> m_TemporaryResources;
	std::vector<Descriptor> m_TemporaryDescriptors;
	std::unordered_map<std::string, RenderResourceHandle> m_StagedTextures;
};