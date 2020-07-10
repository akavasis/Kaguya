#pragma once
#include <vector>
#include <unordered_set>
#include <unordered_map>

#include "RenderDevice.h"

class TextureAllocator;

class GpuTextureAllocator
{
public:
	GpuTextureAllocator(RenderDevice& RefRenderDevice);

	// Stage/Upload all the texture data in the pTextureAllocator to the GPU
	void Stage(TextureAllocator* pTextureAllocator, RenderCommandContext* pRenderCommandContext);
	void Bind(RenderCommandContext* pRenderCommandContext) const;
private:
	void GenerateMips(RenderResourceHandle TextureHandle, RenderCommandContext* pRenderCommandContext);
	void GenerateMipsUAV(RenderResourceHandle TextureHandle, RenderCommandContext* pRenderCommandContext);
	void GenerateMipsSRGB(RenderResourceHandle TextureHandle, RenderCommandContext* pRenderCommandContext);

	bool IsUAVCompatable(DXGI_FORMAT Format);
	bool IsSRGB(DXGI_FORMAT Format);
	DXGI_FORMAT GetUAVCompatableFormat(DXGI_FORMAT Format);

	RenderDevice& m_RefRenderDevice;

	DescriptorAllocator m_TextureViewAllocator;
	Descriptor m_MipmapNullUAV;

	std::unordered_set<DXGI_FORMAT> m_UAVSupportedFormat;

	std::vector<RenderResourceHandle> m_TemporaryResources;
	std::vector<RenderResourceHandle> m_StagedTextures;
};