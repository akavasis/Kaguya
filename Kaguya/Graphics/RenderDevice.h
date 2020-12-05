#pragma once
#include <wil/resource.h>
#include <wrl/client.h>
#include <d3d12.h>

#include <functional>
#include <Template/Pool.h>
#include "DXGIManager.h"
#include "RenderResourceHandle.h"
#include "RenderResourceHandleRegistry.h"
#include "RenderBuffer.h"
#include "RenderTexture.h"
#include "RenderResourceContainer.h"

#include "API/D3D12/Device.h"
#include "API/D3D12/ShaderCompiler.h"
#include "API/D3D12/ResourceStateTracker.h"
#include "API/D3D12/DescriptorHeap.h"
#include "API/D3D12/Fence.h"
#include "API/D3D12/CommandQueue.h"
#include "API/D3D12/CommandContext.h"
#include "API/D3D12/RaytracingAccelerationStructure.h"

#include "API/Proxy/BufferProxy.h"
#include "API/Proxy/TextureProxy.h"
#include "API/Proxy/HeapProxy.h"
#include "API/Proxy/RootSignatureProxy.h"
#include "API/Proxy/PipelineStateProxy.h"
#include "API/Proxy/RaytracingPipelineStateProxy.h"

struct RootParameters
{
	struct ShaderLayout
	{
		enum
		{
			SystemConstants,
			RenderPassDataCB,
			ShaderResourceDescriptorTable,
			UnorderedAccessDescriptorTable,
			SamplerDescriptorTable,
			NumRootParameters
		};
	};
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

	static constexpr DXGI_FORMAT SwapChainBufferFormat	= DXGI_FORMAT_R8G8B8A8_UNORM;
	static constexpr DXGI_FORMAT DepthFormat			= DXGI_FORMAT_D32_FLOAT;
	static constexpr DXGI_FORMAT DepthStencilFormat		= DXGI_FORMAT_D24_UNORM_S8_UINT;

	RenderDevice(IDXGIAdapter4* pAdapter);
	~RenderDevice();

	[[nodiscard]] CommandContext* AllocateContext(CommandContext::Type Type);

	void BindGpuDescriptorHeap(CommandContext* pCommandContext);
	inline auto GetGpuDescriptorHeapCBDescriptorFromStart() const { return m_ShaderVisibleCBSRUADescriptorHeap.GetCBDescriptorAt(0); }
	inline auto GetGpuDescriptorHeapSRDescriptorFromStart() const { return m_ShaderVisibleCBSRUADescriptorHeap.GetSRDescriptorAt(0); }
	inline auto GetGpuDescriptorHeapUADescriptorFromStart() const { return m_ShaderVisibleCBSRUADescriptorHeap.GetUADescriptorAt(0); }
	inline auto GetSamplerDescriptorHeapDescriptorFromStart() const { return m_SamplerDescriptorHeap.GetDescriptorFromStart(); }
	void ExecuteCommandContexts(CommandContext::Type Type, UINT NumCommandContexts, CommandContext* ppCommandContexts[]);

	// Shader compilation
	[[nodiscard]] Shader CompileShader(Shader::Type Type, const std::filesystem::path& Path, LPCWSTR pEntryPoint, const std::vector<DxcDefine>& ShaderDefines);
	[[nodiscard]] Library CompileLibrary(const std::filesystem::path& Path);

	RenderResourceHandle InitializeRenderResourceHandle(RenderResourceType Type, const std::string& Name);
	std::string GetRenderResourceHandleName(RenderResourceHandle RenderResourceHandle);

	// Resource creation
	void CreateBuffer(RenderResourceHandle Handle, std::function<void(BufferProxy&)> Configurator);
	void CreateBuffer(RenderResourceHandle Handle, RenderResourceHandle HeapHandle, UINT64 HeapOffset, std::function<void(BufferProxy&)> Configurator);

	void CreateTexture(RenderResourceHandle Handle, Microsoft::WRL::ComPtr<ID3D12Resource> ExistingResource, Resource::State InitialState);
	void CreateTexture(RenderResourceHandle Handle, Resource::Type Type, std::function<void(TextureProxy&)> Configurator);
	void CreateTexture(RenderResourceHandle Handle, Resource::Type Type, RenderResourceHandle HeapHandle, UINT64 HeapOffset, std::function<void(TextureProxy&)> Configurator);

	void CreateHeap(RenderResourceHandle Handle, std::function<void(HeapProxy&)> Configurator);

	void CreateRootSignature(RenderResourceHandle Handle, std::function<void(RootSignatureProxy&)> Configurator, bool AddShaderLayoutRootParameters = true);
	
	void CreateGraphicsPipelineState(RenderResourceHandle Handle, std::function<void(GraphicsPipelineStateProxy&)> Configurator);
	void CreateComputePipelineState(RenderResourceHandle Handle, std::function<void(ComputePipelineStateProxy&)> Configurator);
	void CreateRaytracingPipelineState(RenderResourceHandle Handle, std::function<void(RaytracingPipelineStateProxy&)> Configurator);

	void Destroy(RenderResourceHandle Handle);

	// Resource view creation
	void CreateShaderResourceView(RenderResourceHandle Handle, std::optional<UINT> MostDetailedMip = {}, std::optional<UINT> MipLevels = {});
	void CreateUnorderedAccessView(RenderResourceHandle Handle, std::optional<UINT> ArraySlice = {}, std::optional<UINT> MipSlice = {});
	void CreateRenderTargetView(RenderResourceHandle Handle, std::optional<UINT> ArraySlice = {}, std::optional<UINT> MipSlice = {}, std::optional<UINT> ArraySize = {});
	void CreateDepthStencilView(RenderResourceHandle Handle, std::optional<UINT> ArraySlice = {}, std::optional<UINT> MipSlice = {}, std::optional<UINT> ArraySize = {});

	// Returns nullptr if a resource is not found
	[[nodiscard]] inline auto GetBuffer(RenderResourceHandle Handle)		{ return m_Buffers.GetResource(Handle); }
	[[nodiscard]] inline auto GetTexture(RenderResourceHandle Handle)		{ return m_Textures.GetResource(Handle); }
	[[nodiscard]] inline auto GetHeap(RenderResourceHandle Handle)			{ return m_Heaps.GetResource(Handle); }
	[[nodiscard]] inline auto GetRootSignature(RenderResourceHandle Handle)	{ return m_RootSignatures.GetResource(Handle); }
	[[nodiscard]] inline auto GetGraphicsPSO(RenderResourceHandle Handle)	{ return m_GraphicsPipelineStates.GetResource(Handle); }
	[[nodiscard]] inline auto GetComputePSO(RenderResourceHandle Handle)	{ return m_ComputePipelineStates.GetResource(Handle); }
	[[nodiscard]] inline auto GetRaytracingPSO(RenderResourceHandle Handle)	{ return m_RaytracingPipelineStates.GetResource(Handle); }

	Descriptor GetShaderResourceView(RenderResourceHandle Handle, std::optional<UINT> MostDetailedMip = {}, std::optional<UINT> MipLevels = {}) const;
	Descriptor GetUnorderedAccessView(RenderResourceHandle Handle, std::optional<UINT> ArraySlice = {}, std::optional<UINT> MipSlice = {}) const;
	Descriptor GetRenderTargetView(RenderResourceHandle Handle, std::optional<UINT> ArraySlice = {}, std::optional<UINT> MipSlice = {}, std::optional<UINT> ArraySize = {}) const;
	Descriptor GetDepthStencilView(RenderResourceHandle Handle, std::optional<UINT> ArraySlice = {}, std::optional<UINT> MipSlice = {}, std::optional<UINT> ArraySize = {}) const;

	Device														Device;
	CommandQueue												GraphicsQueue, ComputeQueue, CopyQueue;
	Microsoft::WRL::ComPtr<ID3D12Fence1>						GraphicsFence, ComputeFence, CopyFence;
	UINT64														GraphicsFenceValue, ComputeFenceValue, CopyFenceValue;
	wil::unique_event											GraphicsFenceCompletionEvent, ComputeFenceCompletionEvent, CopyFenceCompletionEvent;
	ResourceStateTracker										GlobalResourceStateTracker;
	ShaderCompiler												ShaderCompiler;

	UINT														BackBufferIndex;
	RenderResourceHandle										BackBufferHandle[NumSwapChainBuffers];
private:
	CommandQueue* GetApiCommandQueue(CommandContext::Type Type);
	void AddShaderLayoutRootParameter(RootSignatureProxy& RootSignatureProxy);

	std::vector<std::unique_ptr<CommandContext>>				m_CommandContexts[CommandContext::NumTypes];

	RenderResourceHandleRegistry								m_BufferHandleRegistry;
	RenderResourceHandleRegistry								m_TextureHandleRegistry;
	RenderResourceHandleRegistry								m_HeapHandleRegistry;
	RenderResourceHandleRegistry								m_RootSignatureHandleRegistry;
	RenderResourceHandleRegistry								m_GraphicsPipelineStateHandleRegistry;
	RenderResourceHandleRegistry								m_ComputePipelineStateHandleRegistry;
	RenderResourceHandleRegistry								m_RaytracingPipelineStateHandleRegistry;

	RenderResourceContainer<Buffer>								m_Buffers;
	RenderResourceContainer<Texture>							m_Textures;
	RenderResourceContainer<Heap>								m_Heaps;
	RenderResourceContainer<RootSignature>						m_RootSignatures;
	RenderResourceContainer<GraphicsPipelineState>				m_GraphicsPipelineStates;
	RenderResourceContainer<ComputePipelineState>				m_ComputePipelineStates;
	RenderResourceContainer<RaytracingPipelineState>			m_RaytracingPipelineStates;

	CBSRUADescriptorHeap										m_NonShaderVisibleCBSRUADescriptorHeap;
	CBSRUADescriptorHeap										m_ShaderVisibleCBSRUADescriptorHeap;
	SamplerDescriptorHeap										m_SamplerDescriptorHeap;
	RenderTargetDescriptorHeap									m_RenderTargetDescriptorHeap;
	DepthStencilDescriptorHeap									m_DepthStencilDescriptorHeap;

	Pool<void, NumConstantBufferDescriptors>					m_ConstantBufferDescriptorIndexPool;
	Pool<void, NumShaderResourceDescriptors>					m_ShaderResourceDescriptorIndexPool;
	Pool<void, NumUnorderedAccessDescriptors>					m_UnorderedAccessDescriptorIndexPool;
	Pool<void, NumSamplerDescriptors>							m_SamplerDescriptorIndexPool;
	Pool<void, NumRenderTargetDescriptors>						m_RenderTargetDescriptorIndexPool;
	Pool<void, NumDepthStencilDescriptors>						m_DepthStencilDescriptorIndexPool;

	std::unordered_map<RenderResourceHandle, RenderBuffer>		m_RenderBuffers;
	std::unordered_map<RenderResourceHandle, RenderTexture>		m_RenderTextures;
};