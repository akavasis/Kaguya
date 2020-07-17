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
	GpuTextureAllocator(std::size_t NumMaterials, RenderDevice* pRenderDevice);

	inline auto GetBRDFLUTIndex() const { return 0; }
	inline auto GetSkyboxEquirectangularMapIndex() const { return 1; }
	inline auto GetSkyboxCubemapIndex() const { return 2; }
	inline auto GetSkyboxIrradianceCubemapIndex() const { return 3; }
	inline auto GetSkyboxPrefilteredCubemapIndex() const { return 4; }

	void Initialize(RenderCommandContext* pRenderCommandContext);
	void StageSkybox(Scene& Scene, RenderCommandContext* pRenderCommandContext);
	void Stage(Scene& Scene, RenderCommandContext* pRenderCommandContext);
	void Update(Scene& Scene);
	void Bind(UINT MaterialTextureIndicesRootParameterIndex, UINT StandardDescriptorTablesRootParameterIndex, RenderCommandContext* pRenderCommandContext) const;
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
		std::size_t index;															// material texture index
		bool generateMips;															// indicates whether or not we should generate mips for the staged texture
	};

	RenderResourceHandle LoadFromFile(const std::filesystem::path& Path, bool ForceSRGB, bool GenerateMips);
	void LoadMaterial(Material& Material);
	void StageTexture(RenderResourceHandle TextureHandle, StagingTexture& StagingTexture, RenderCommandContext* pRenderCommandContext);
	void GenerateMips(RenderResourceHandle TextureHandle, RenderCommandContext* pRenderCommandContext);
	void GenerateMipsUAV(RenderResourceHandle TextureHandle, RenderCommandContext* pRenderCommandContext);
	void GenerateMipsSRGB(RenderResourceHandle TextureHandle, RenderCommandContext* pRenderCommandContext);
	void EquirectangularToCubemap(RenderResourceHandle EquirectangularMap, RenderResourceHandle Cubemap, RenderCommandContext* pRenderCommandContext);
	void EquirectangularToCubemapUAV(RenderResourceHandle EquirectangularMap, RenderResourceHandle Cubemap, RenderCommandContext* pRenderCommandContext);
	void EquirectangularToCubemapSRGB(RenderResourceHandle EquirectangularMap, RenderResourceHandle Cubemap, RenderCommandContext* pRenderCommandContext);

	bool IsUAVCompatable(DXGI_FORMAT Format);
	bool IsSRGB(DXGI_FORMAT Format);
	DXGI_FORMAT GetUAVCompatableFormat(DXGI_FORMAT Format);

	RenderDevice* m_pRenderDevice;

	enum : UINT { NumDescriptorsPerRange = 2048 };
	CBSRUADescriptorHeap m_StagingDescriptorHeap;
	DescriptorAllocation m_TextureSRVs;
	DescriptorAllocation m_RTV;

	std::unordered_set<DXGI_FORMAT> m_UAVSupportedFormat;

	// TODO: Add method to free them after Stage call
	std::unordered_map<RenderResourceHandle, StagingTexture> m_UnstagedTextures;
	std::vector<RenderResourceHandle> m_TemporaryResources;
	std::vector<DescriptorAllocation> m_TemporaryDescriptorAllocations;

	RenderResourceHandle m_CubemapCamerasUploadBufferHandle;
	RenderResourceHandle m_BRDFLUTHandle;
	RenderResourceHandle m_SkyboxEquirectangularMapHandle;
	RenderResourceHandle m_SkyboxCubemapHandle;
	RenderResourceHandle m_SkyboxIrradianceCubemapHandle;
	RenderResourceHandle m_SkyboxPrefilteredCubemapHandle;
	struct TextureStorage
	{
		std::unordered_map<std::string, RenderResourceHandle> TextureHandles;
		std::unordered_map<std::string, std::size_t> TextureIndices;

		void AddTexture(std::string Path, RenderResourceHandle AssociatedHandle, std::size_t GpuTextureIndex)
		{
			TextureHandles[Path] = AssociatedHandle;
			TextureIndices[Path] = GpuTextureIndex;
		}

		void RemoveTexture(std::string Path)
		{
			TextureHandles.erase(Path);
			TextureIndices.erase(Path);
		}
	} m_TextureStorage;

	std::size_t m_TextureIndex;

	RenderResourceHandle m_MaterialTextureIndicesStructuredBufferHandle;
	Buffer* m_pMaterialTextureIndicesStructuredBuffer;
};