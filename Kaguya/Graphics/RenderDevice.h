#pragma once
#include "Core/Delegate.h"

#include "AL/D3D12/Device.h"
#include "AL/D3D12/ShaderCompiler.h"
#include "AL/D3D12/ResourceStateTracker.h"
#include "AL/D3D12/DescriptorAllocator.h"
#include "AL/D3D12/CommandQueue.h"
#include "AL/D3D12/CommandContext.h"

#include "AL/Proxy/BufferProxy.h"
#include "AL/Proxy/TextureProxy.h"
#include "AL/Proxy/HeapProxy.h"
#include "AL/Proxy/RootSignatureProxy.h"
#include "AL/Proxy/PipelineStateProxy.h"
#include "AL/Proxy/RaytracingPipelineStateProxy.h"

#include "RenderResourceContainer.h"

// Standard Shader Layout is added after Configurator call
struct StandardShaderLayoutOptions
{
	BOOL InitConstantDataTypeAsRootConstants;
	UINT Num32BitValues;
};

/*
	Abstraction of underlying GPU Device, its able to create gpu resources
	and contains underlying gpu resources as well, resources are referred by using
	RenderResourceHandles
*/
class RenderDevice
{
public:
	RenderDevice(IDXGIAdapter4* pAdapter);
	~RenderDevice();

	[[nodiscard]] CommandContext* AllocateContext(CommandContext::Type Type);

	void BindUniversalGpuDescriptorHeap(CommandContext* pCommandContext) const;
	auto GetUniversalGpuDescriptorHeapSRVDescriptorHandleFromStart() const
	{
		return DescriptorAllocator.GetUniversalGpuDescriptorHeap()->GetRangeHandleFromStart(CBSRUADescriptorHeap::RangeType::ShaderResource).GPUHandle;
	}
	auto GetUniversalGpuDescriptorHeapUAVDescriptorHandleFromStart() const
	{
		return DescriptorAllocator.GetUniversalGpuDescriptorHeap()->GetRangeHandleFromStart(CBSRUADescriptorHeap::RangeType::UnorderedAccess).GPUHandle;
	}
	void ExecuteRenderCommandContexts(UINT NumCommandContexts, CommandContext* ppCommandContexts[]);

	// Shader compilation
	[[nodiscard]] RenderResourceHandle CompileShader(Shader::Type Type, const std::filesystem::path& Path, LPCWSTR pEntryPoint, const std::vector<DxcDefine>& ShaderDefines);
	[[nodiscard]] RenderResourceHandle CompileLibrary(const std::filesystem::path& Path);

	// Resource creation
	[[nodiscard]] RenderResourceHandle CreateBuffer(Delegate<void(BufferProxy&)> Configurator);
	[[nodiscard]] RenderResourceHandle CreateBuffer(RenderResourceHandle HeapHandle, UINT64 HeapOffset, Delegate<void(BufferProxy&)> Configurator);

	[[nodiscard]] RenderResourceHandle CreateTexture(Microsoft::WRL::ComPtr<ID3D12Resource> ExistingResource, Resource::State InitialState);
	[[nodiscard]] RenderResourceHandle CreateTexture(Resource::Type Type, Delegate<void(TextureProxy&)> Configurator);
	[[nodiscard]] RenderResourceHandle CreateTexture(Resource::Type Type, RenderResourceHandle HeapHandle, UINT64 HeapOffset, Delegate<void(TextureProxy&)> Configurator);

	[[nodiscard]] RenderResourceHandle CreateHeap(Delegate<void(HeapProxy&)> Configurator);

	// If pOptions is not null, AppendStandardShaderLayoutRootParameter will be called after Configurator
	[[nodiscard]] RenderResourceHandle CreateRootSignature(StandardShaderLayoutOptions* pOptions, Delegate<void(RootSignatureProxy&)> Configurator);
	[[nodiscard]] RenderResourceHandle CreateGraphicsPipelineState(Delegate<void(GraphicsPipelineStateProxy&)> Configurator);
	[[nodiscard]] RenderResourceHandle CreateComputePipelineState(Delegate<void(ComputePipelineStateProxy&)> Configurator);
	[[nodiscard]] RenderResourceHandle CreateRaytracingPipelineState(Delegate<void(RaytracingPipelineStateProxy&)> Configurator);

	void Destroy(RenderResourceHandle* pRenderResourceHandle);

	// Resource view creation
	void CreateSRV(RenderResourceHandle RenderResourceHandle, Descriptor DestDescriptor);
	void CreateUAV(RenderResourceHandle RenderResourceHandle, Descriptor DestDescriptor, std::optional<UINT> ArraySlice, std::optional<UINT> MipSlice);
	void CreateRTV(RenderResourceHandle RenderResourceHandle, Descriptor DestDescriptor, std::optional<UINT> ArraySlice, std::optional<UINT> MipSlice, std::optional<UINT> ArraySize);
	void CreateDSV(RenderResourceHandle RenderResourceHandle, Descriptor DestDescriptor, std::optional<UINT> ArraySlice, std::optional<UINT> MipSlice, std::optional<UINT> ArraySize);

	// Returns nullptr if a resource is not found
	[[nodiscard]] inline auto GetShader(RenderResourceHandle RenderResourceHandle) { return m_Shaders.GetResource(RenderResourceHandle); }
	[[nodiscard]] inline auto GetLibrary(RenderResourceHandle RenderResourceHandle) { return m_Libraries.GetResource(RenderResourceHandle); }
	[[nodiscard]] inline auto GetBuffer(RenderResourceHandle RenderResourceHandle) { return m_Buffers.GetResource(RenderResourceHandle); }
	[[nodiscard]] inline auto GetTexture(RenderResourceHandle RenderResourceHandle) { return m_Textures.GetResource(RenderResourceHandle); }
	[[nodiscard]] inline auto GetHeap(RenderResourceHandle RenderResourceHandle) { return m_Heaps.GetResource(RenderResourceHandle); }
	[[nodiscard]] inline auto GetRootSignature(RenderResourceHandle RenderResourceHandle) { return m_RootSignatures.GetResource(RenderResourceHandle); }
	[[nodiscard]] inline auto GetGraphicsPSO(RenderResourceHandle RenderResourceHandle) { return m_GraphicsPipelineStates.GetResource(RenderResourceHandle); }
	[[nodiscard]] inline auto GetComputePSO(RenderResourceHandle RenderResourceHandle) { return m_ComputePipelineStates.GetResource(RenderResourceHandle); }
	[[nodiscard]] inline auto GetRaytracingPSO(RenderResourceHandle RenderResourceHandle) { return m_RaytracingPipelineStates.GetResource(RenderResourceHandle); }

	Device Device;
	CommandQueue GraphicsQueue;
	CommandQueue ComputeQueue;
	CommandQueue CopyQueue;
	ResourceStateTracker GlobalResourceStateTracker;
	DescriptorAllocator DescriptorAllocator;
private:
	void AppendStandardShaderLayoutRootParameter(StandardShaderLayoutOptions* pOptions, RootSignatureProxy& RootSignatureProxy);

	ShaderCompiler m_ShaderCompiler;
	std::vector<std::unique_ptr<CommandContext>> m_RenderCommandContexts[CommandContext::NumTypes];

	enum DescriptorRanges
	{
		Tex2DTableRange,
		Tex2DArrayTableRange,
		TexCubeTableRange,
		RawBufferTableRange,

		NumDescriptorRanges
	};
	std::array<D3D12_DESCRIPTOR_RANGE1, std::size_t(DescriptorRanges::NumDescriptorRanges)> m_StandardShaderLayoutDescriptorRanges;

	RenderResourceContainer<RenderResourceType::Shader, Shader> m_Shaders;
	RenderResourceContainer<RenderResourceType::Library, Library> m_Libraries;

	RenderResourceContainer<RenderResourceType::Buffer, Buffer> m_Buffers;
	RenderResourceContainer<RenderResourceType::Texture, Texture> m_Textures;

	RenderResourceContainer<RenderResourceType::Heap, Heap> m_Heaps;
	RenderResourceContainer<RenderResourceType::RootSignature, RootSignature> m_RootSignatures;
	RenderResourceContainer<RenderResourceType::GraphicsPSO, GraphicsPipelineState> m_GraphicsPipelineStates;
	RenderResourceContainer<RenderResourceType::ComputePSO, ComputePipelineState> m_ComputePipelineStates;
	RenderResourceContainer<RenderResourceType::RaytracingPSO, RaytracingPipelineState> m_RaytracingPipelineStates;
};