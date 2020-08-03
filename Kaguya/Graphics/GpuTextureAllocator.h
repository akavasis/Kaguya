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
	enum AssetTextures
	{
		BRDFLUT,
		SkyboxEquirectangularMap,
		SkyboxCubemap,
		SkyboxIrradianceCubemap,
		SkyboxPrefilteredCubemap,
		NumAssetTextures
	};
	RenderResourceHandle AssetTextures[NumAssetTextures];

	struct TextureStorage
	{
		std::unordered_map<std::string, RenderResourceHandle> TextureHandles;
		std::unordered_map<std::string, size_t> TextureIndices;

		std::size_t Size() const;
		void AddTexture(const std::string& Path, RenderResourceHandle AssociatedHandle, std::size_t GpuTextureIndex);
		void RemoveTexture(const std::string& Path);
		std::pair<RenderResourceHandle, size_t> GetTexture(const std::string& Path) const;
	} TextureStorage;

	GpuTextureAllocator(RenderDevice* pRenderDevice, size_t NumMaterials);

	inline auto GetNumTextures() const { return m_NumTextures; }

	void Stage(Scene& Scene, CommandContext* pCommandContext);
	void Update(Scene& Scene);
	void Bind(std::optional<UINT> MaterialTextureIndicesRootParameterIndex, std::optional<UINT> MaterialTexturePropertiesRootParameterIndex, CommandContext* pCommandContext) const;
private:
	struct Status
	{
		bool BRDFGenerated = false;
	};

	struct StagingTexture
	{
		std::string path;															// file path
		Texture texture;															// gpu upload buffer
		std::size_t numSubresources;												// number of subresources
		std::vector<D3D12_PLACED_SUBRESOURCE_FOOTPRINT> placedSubresourceLayouts;	// footprint is synonymous to layout
		std::size_t mipLevels;														// indicates what mip levels this texture should have
		std::size_t index;															// material texture index
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

	Status m_Status;

	enum : UINT { NumDescriptorsPerRange = 2048 };
	CBSRUADescriptorHeap m_CBSRUADescriptorHeap;
	DescriptorAllocation m_RTV;

	std::unordered_set<DXGI_FORMAT> m_UAVSupportedFormat;

	// TODO: Add method to free them after Stage call
	std::unordered_map<RenderResourceHandle, StagingTexture> m_UnstagedTextures;
	std::vector<RenderResourceHandle> m_TemporaryResources;
	std::vector<DescriptorAllocation> m_TemporaryDescriptorAllocations;

	RenderResourceHandle m_CubemapCamerasUploadBufferHandle;

	size_t m_NumTextures;

	RenderResourceHandle m_MaterialTextureIndicesStructuredBufferHandle;
	Buffer* m_pMaterialTextureIndicesStructuredBuffer;
	RenderResourceHandle m_MaterialTexturePropertiesStructuredBufferHandle;
	Buffer* m_pMaterialTexturePropertiesStructuredBuffer;
};