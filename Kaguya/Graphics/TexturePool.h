#pragma once
#include <vector>
#include <unordered_set>
#include <unordered_map>
#include <filesystem>

#include <DirectXTex.h>

#include "RenderDevice.h"

class TexturePool
{
public:
	TexturePool(std::filesystem::path ExecutableFolderPath, RenderDevice& RefRenderDevice);

	void Stage(RenderCommandContext* pRenderCommandContext);

	[[nodiscard]] RenderResourceHandle LoadFromFile(const char* pPath, bool ForceSRGB, bool GenerateMips);

private:
	void GenerateMips(RenderResourceHandle TextureHandle, RenderCommandContext* pRenderCommandContext);
	void GenerateMipsUAV(RenderResourceHandle TextureHandle, RenderCommandContext* pRenderCommandContext);
	void GenerateMipsSRGB(RenderResourceHandle TextureHandle, RenderCommandContext* pRenderCommandContext);
	
	bool IsUAVCompatable(DXGI_FORMAT Format);
	bool IsSRGB(DXGI_FORMAT Format);
	DXGI_FORMAT GetUAVCompatableFormat(DXGI_FORMAT Format);

	std::filesystem::path m_ExecutableFolderPath;
	RenderDevice& m_RefRenderDevice;

	struct StagingTexture
	{
		Texture texture;
		DirectX::ScratchImage scratchImage;
		std::string fileName;
		bool generateMips = false;
	};

	std::unordered_map<RenderResourceHandle, StagingTexture> m_UnstagedTextures;
	std::vector<RenderResourceHandle> m_TemporaryResources;

	std::vector<RenderResourceHandle> m_StagedTextures;

	DescriptorAllocator m_TextureViewAllocator;

	// Default (no resource) UAV's to pad the unused UAV descriptors.
	// If generating less than 4 mip map levels, the unused mip maps
	// need to be padded with default UAVs (to keep the DX12 runtime happy).
	Descriptor m_MipMapDefaultUAV;

	std::unordered_set<DXGI_FORMAT> m_UAVSupportedFormat;
};