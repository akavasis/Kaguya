#pragma once
#include "Template/Pool.h"
#include "RenderBuffer.h"
#include "RenderTexture.h"
#include "RenderResourceContainer.h"

#include "AL/D3D12/Device.h"
#include "AL/D3D12/ShaderCompiler.h"
#include "AL/D3D12/ResourceStateTracker.h"
#include "AL/D3D12/DescriptorHeap.h"
#include "AL/D3D12/CommandQueue.h"
#include "AL/D3D12/CommandContext.h"
#include "AL/D3D12/RaytracingAccelerationStructure.h"

#include "AL/Proxy/BufferProxy.h"
#include "AL/Proxy/TextureProxy.h"
#include "AL/Proxy/HeapProxy.h"
#include "AL/Proxy/RootSignatureProxy.h"
#include "AL/Proxy/PipelineStateProxy.h"
#include "AL/Proxy/RaytracingPipelineStateProxy.h"

struct RaytracingAccelerationStructureHandles
{
	RenderResourceHandle Scratch;
	RenderResourceHandle Result;
	RenderResourceHandle InstanceDescs;
};

/*
	Abstraction of underlying GPU Device, its able to create gpu resources
	and contains underlying gpu resources as well, resources are referred by using
	RenderResourceHandles
*/
class RenderDevice
{
public:
	enum
	{
		NumSwapChainBuffers				= 3,
		NumConstantBufferDescriptors	= 1024,
		NumShaderResourceDescriptors	= 1024,
		NumUnorderedAccessDescriptors	= 1024,
		NumSamplerDescriptors			= 1024,
		NumRenderTargetDescriptors		= 1024,
		NumDepthStencilDescriptors		= 1024
	};

	RenderDevice(IDXGIAdapter4* pAdapter);

	[[nodiscard]] CommandContext* AllocateContext(CommandContext::Type Type);

	void BindUniversalGpuDescriptorHeap(CommandContext* pCommandContext);
	inline auto GetGpuDescriptorHeapCBDescriptorFromStart() const { return m_CBSRUADescriptorHeap.GetCBDescriptorAt(0); }
	inline auto GetGpuDescriptorHeapSRDescriptorFromStart() const { return m_CBSRUADescriptorHeap.GetSRDescriptorAt(0); }
	inline auto GetGpuDescriptorHeapUADescriptorFromStart() const { return m_CBSRUADescriptorHeap.GetUADescriptorAt(0); }
	inline auto GetSamplerDescriptorHeapDescriptorFromStart() const { return m_SamplerDescriptorHeap.GetDescriptorFromStart(); }
	void ExecuteRenderCommandContexts(UINT NumCommandContexts, CommandContext* ppCommandContexts[]);

	// Shader compilation
	[[nodiscard]] Shader CompileShader(Shader::Type Type, const std::filesystem::path& Path, LPCWSTR pEntryPoint, const std::vector<DxcDefine>& ShaderDefines);
	[[nodiscard]] Library CompileLibrary(const std::filesystem::path& Path);

	// Resource creation
	[[nodiscard]] RenderResourceHandle CreateBuffer(std::function<void(BufferProxy&)> Configurator);
	[[nodiscard]] RenderResourceHandle CreateBuffer(RenderResourceHandle HeapHandle, UINT64 HeapOffset, std::function<void(BufferProxy&)> Configurator);

	[[nodiscard]] RenderResourceHandle CreateTexture(Microsoft::WRL::ComPtr<ID3D12Resource> ExistingResource, Resource::State InitialState);
	[[nodiscard]] RenderResourceHandle CreateTexture(Resource::Type Type, std::function<void(TextureProxy&)> Configurator);
	[[nodiscard]] RenderResourceHandle CreateTexture(Resource::Type Type, RenderResourceHandle HeapHandle, UINT64 HeapOffset, std::function<void(TextureProxy&)> Configurator);

	[[nodiscard]] RenderResourceHandle CreateHeap(std::function<void(HeapProxy&)> Configurator);

	[[nodiscard]] RenderResourceHandle CreateRootSignature(std::function<void(RootSignatureProxy&)> Configurator, bool AddShaderLayoutRootParameters = true);
	[[nodiscard]] RenderResourceHandle CreateGraphicsPipelineState(std::function<void(GraphicsPipelineStateProxy&)> Configurator);
	[[nodiscard]] RenderResourceHandle CreateComputePipelineState(std::function<void(ComputePipelineStateProxy&)> Configurator);
	[[nodiscard]] RenderResourceHandle CreateRaytracingPipelineState(std::function<void(RaytracingPipelineStateProxy&)> Configurator);

	void Destroy(RenderResourceHandle* pRenderResourceHandle);

	// Resource view creation
	void CreateShaderResourceView(RenderResourceHandle RenderResourceHandle, std::optional<UINT> MostDetailedMip = {}, std::optional<UINT> MipLevels = {});
	void CreateUnorderedAccessView(RenderResourceHandle RenderResourceHandle, std::optional<UINT> ArraySlice = {}, std::optional<UINT> MipSlice = {});
	void CreateRenderTargetView(RenderResourceHandle RenderResourceHandle, std::optional<UINT> ArraySlice = {}, std::optional<UINT> MipSlice = {}, std::optional<UINT> ArraySize = {});
	void CreateDepthStencilView(RenderResourceHandle RenderResourceHandle, std::optional<UINT> ArraySlice = {}, std::optional<UINT> MipSlice = {}, std::optional<UINT> ArraySize = {});

	// Returns nullptr if a resource is not found
	[[nodiscard]] inline auto GetBuffer(RenderResourceHandle RenderResourceHandle) { return m_Buffers.GetResource(RenderResourceHandle); }
	[[nodiscard]] inline auto GetTexture(RenderResourceHandle RenderResourceHandle) { return m_Textures.GetResource(RenderResourceHandle); }
	[[nodiscard]] inline auto GetHeap(RenderResourceHandle RenderResourceHandle) { return m_Heaps.GetResource(RenderResourceHandle); }
	[[nodiscard]] inline auto GetRootSignature(RenderResourceHandle RenderResourceHandle) { return m_RootSignatures.GetResource(RenderResourceHandle); }
	[[nodiscard]] inline auto GetGraphicsPSO(RenderResourceHandle RenderResourceHandle) { return m_GraphicsPipelineStates.GetResource(RenderResourceHandle); }
	[[nodiscard]] inline auto GetComputePSO(RenderResourceHandle RenderResourceHandle) { return m_ComputePipelineStates.GetResource(RenderResourceHandle); }
	[[nodiscard]] inline auto GetRaytracingPSO(RenderResourceHandle RenderResourceHandle) { return m_RaytracingPipelineStates.GetResource(RenderResourceHandle); }

	Descriptor GetShaderResourceView(RenderResourceHandle RenderResourceHandle, std::optional<UINT> MostDetailedMip = {}, std::optional<UINT> MipLevels = {}) const;
	Descriptor GetUnorderedAccessView(RenderResourceHandle RenderResourceHandle, std::optional<UINT> ArraySlice = {}, std::optional<UINT> MipSlice = {}) const;
	Descriptor GetRenderTargetView(RenderResourceHandle RenderResourceHandle, std::optional<UINT> ArraySlice = {}, std::optional<UINT> MipSlice = {}, std::optional<UINT> ArraySize = {}) const;
	Descriptor GetDepthStencilView(RenderResourceHandle RenderResourceHandle, std::optional<UINT> ArraySlice = {}, std::optional<UINT> MipSlice = {}, std::optional<UINT> ArraySize = {}) const;

	Device Device;
	CommandQueue GraphicsQueue;
	CommandQueue ComputeQueue;
	CommandQueue CopyQueue;
	ResourceStateTracker GlobalResourceStateTracker;

	UINT FrameIndex;
	RenderResourceHandle SwapChainTextures[NumSwapChainBuffers];
private:
	void AddShaderLayoutRootParameter(RootSignatureProxy& RootSignatureProxy);

	ShaderCompiler m_ShaderCompiler;
	std::vector<std::unique_ptr<CommandContext>> m_RenderCommandContexts[CommandContext::NumTypes];

	RenderResourceContainer<RenderResourceType::Buffer, Buffer> m_Buffers;
	RenderResourceContainer<RenderResourceType::Texture, Texture> m_Textures;

	RenderResourceContainer<RenderResourceType::Heap, Heap> m_Heaps;
	RenderResourceContainer<RenderResourceType::RootSignature, RootSignature> m_RootSignatures;
	RenderResourceContainer<RenderResourceType::GraphicsPSO, GraphicsPipelineState> m_GraphicsPipelineStates;
	RenderResourceContainer<RenderResourceType::ComputePSO, ComputePipelineState> m_ComputePipelineStates;
	RenderResourceContainer<RenderResourceType::RaytracingPSO, RaytracingPipelineState> m_RaytracingPipelineStates;

	CBSRUADescriptorHeap m_CBSRUADescriptorHeap;
	SamplerDescriptorHeap m_SamplerDescriptorHeap;
	RenderTargetDescriptorHeap m_RenderTargetDescriptorHeap;
	DepthStencilDescriptorHeap m_DepthStencilDescriptorHeap;

	Pool<void, NumConstantBufferDescriptors> m_ConstantBufferDescriptorIndexPool;
	Pool<void, NumShaderResourceDescriptors> m_ShaderResourceDescriptorIndexPool;
	Pool<void, NumUnorderedAccessDescriptors> m_UnorderedAccessDescriptorIndexPool;
	Pool<void, NumSamplerDescriptors> m_SamplerDescriptorIndexPool;
	Pool<void, NumRenderTargetDescriptors> m_RenderTargetDescriptorIndexPool;
	Pool<void, NumDepthStencilDescriptors> m_DepthStencilDescriptorIndexPool;

	std::unordered_map<RenderResourceHandle, RenderBuffer> m_RenderBuffers;
	std::unordered_map<RenderResourceHandle, RenderTexture> m_RenderTextures;
};