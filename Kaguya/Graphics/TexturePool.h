#pragma once
#include <vector>
#include <unordered_map>
#include <filesystem>

#include <DirectXTex.h>

#include "RenderDevice.h"

class TexturePool
{
public:
	TexturePool(std::filesystem::path ExecutableFolderPath, RenderDevice& RefRenderDevice)
		: m_ExecutableFolderPath(ExecutableFolderPath),
		m_RefRenderDevice(RefRenderDevice)
	{}

	void Stage(RenderCommandContext* pRenderCommandContext);

	[[nodiscard]] RenderResourceHandle LoadFromFile(const char* pPath, bool ForceSRGB, bool GenerateMips);

private:
	void GenerateMips(RenderResourceHandle Texture, RenderCommandContext* pRenderCommandContext);

	std::filesystem::path m_ExecutableFolderPath;
	RenderDevice& m_RefRenderDevice;

	struct StagingTexture
	{
		Texture texture;
		bool generateMips = false;
	};

	std::unordered_map<RenderResourceHandle, StagingTexture> m_UnstagedTextures;

	std::vector<RenderResourceHandle> m_StagedTextures;
};