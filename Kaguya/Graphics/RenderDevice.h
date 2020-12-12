#pragma once
#include <wil/resource.h>
#include <wrl/client.h>
#include <d3d12.h>

#include <functional>
#include <Template/Pool.h>
#include "RenderResourceHandle.h"
#include "RenderResourceHandleRegistry.h"
#include "RenderBuffer.h"
#include "RenderTexture.h"
#include "RenderResourceContainer.h"

#include "API/D3D12/Device.h"
#include "API/D3D12/CommandQueue.h"
#include "API/D3D12/ResourceStateTracker.h"
#include "API/D3D12/ShaderCompiler.h"
#include "API/D3D12/DescriptorHeap.h"
#include "API/D3D12/CommandContext.h"
#include "API/D3D12/RaytracingAccelerationStructure.h"

#include "API/D3D12/RootSignatureBuilder.h"

#include "API/Proxy/BufferProxy.h"
#include "API/Proxy/TextureProxy.h"
#include "API/Proxy/PipelineStateProxy.h"
#include "API/Proxy/RaytracingPipelineStateProxy.h"

#include "API/D3D12/Heap.h"

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

		NumConstantBufferDescriptors	= 512,
		NumShaderResourceDescriptors	= 512,
		NumUnorderedAccessDescriptors	= 512,
		NumSamplerDescriptors			= 512,
		NumRenderTargetDescriptors		= 64,
		NumDepthStencilDescriptors		= 64
	};

	static constexpr DXGI_FORMAT SwapChainBufferFormat	= DXGI_FORMAT_R8G8B8A8_UNORM;
	static constexpr DXGI_FORMAT DepthFormat			= DXGI_FORMAT_D32_FLOAT;
	static constexpr DXGI_FORMAT DepthStencilFormat		= DXGI_FORMAT_D24_UNORM_S8_UINT;

	RenderDevice(const Window* pWindow);
	~RenderDevice();

	const std::wstring& GetAdapterDescription() const { return m_AdapterDescription; }
	DXGI_QUERY_VIDEO_MEMORY_INFO QueryLocalVideoMemoryInfo() const;

	void Present(bool VSync);

	void Resize(UINT Width, UINT Height);

	[[nodiscard]] CommandContext* AllocateContext(CommandContext::Type Type);

	void BindGpuDescriptorHeap(CommandContext* pCommandContext);
	inline auto GetGpuDescriptorHeapCBDescriptorFromStart() const { return m_ShaderVisibleCBSRUADescriptorHeap.GetCBDescriptorAt(0); }
	inline auto GetGpuDescriptorHeapSRDescriptorFromStart() const { return m_ShaderVisibleCBSRUADescriptorHeap.GetSRDescriptorAt(0); }
	inline auto GetGpuDescriptorHeapUADescriptorFromStart() const { return m_ShaderVisibleCBSRUADescriptorHeap.GetUADescriptorAt(0); }
	inline auto GetSamplerDescriptorHeapDescriptorFromStart() const { return m_SamplerDescriptorHeap.GetDescriptorFromStart(); }
	void ExecuteCommandContexts(CommandContext::Type Type, UINT NumCommandContexts, CommandContext* ppCommandContexts[]);

	void FlushGraphicsQueue();
	void FlushComputeQueue();
	void FlushCopyQueue();

	RenderResourceHandle InitializeRenderResourceHandle(RenderResourceType Type, const std::string& Name);
	std::string GetRenderResourceHandleName(RenderResourceHandle RenderResourceHandle);

	RenderResourceHandle GetCurrentBackBufferHandle() const { return m_BackBufferHandle[m_BackBufferIndex]; }

	// Resource creation
	void CreateBuffer(RenderResourceHandle Handle, std::function<void(BufferProxy&)> Configurator);
	void CreateBuffer(RenderResourceHandle Handle, RenderResourceHandle HeapHandle, UINT64 HeapOffset, std::function<void(BufferProxy&)> Configurator);

	void CreateTexture(RenderResourceHandle Handle, Microsoft::WRL::ComPtr<ID3D12Resource> ExistingResource, Resource::State InitialState);
	void CreateTexture(RenderResourceHandle Handle, Resource::Type Type, std::function<void(TextureProxy&)> Configurator);
	void CreateTexture(RenderResourceHandle Handle, Resource::Type Type, RenderResourceHandle HeapHandle, UINT64 HeapOffset, std::function<void(TextureProxy&)> Configurator);

	void CreateHeap(RenderResourceHandle Handle, const D3D12_HEAP_DESC& Desc);

	void CreateRootSignature(RenderResourceHandle Handle, std::function<void(RootSignatureBuilder&)> Configurator, bool AddShaderLayoutRootParameters = true);
	
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
	[[nodiscard]] inline auto GetRootSignature(RenderResourceHandle Handle)	{ return m_RootSignatures.GetResource(Handle); }
	[[nodiscard]] inline auto GetGraphicsPSO(RenderResourceHandle Handle)	{ return m_GraphicsPipelineStates.GetResource(Handle); }
	[[nodiscard]] inline auto GetComputePSO(RenderResourceHandle Handle)	{ return m_ComputePipelineStates.GetResource(Handle); }
	[[nodiscard]] inline auto GetRaytracingPSO(RenderResourceHandle Handle)	{ return m_RaytracingPipelineStates.GetResource(Handle); }

	Descriptor GetShaderResourceView(RenderResourceHandle Handle, std::optional<UINT> MostDetailedMip = {}, std::optional<UINT> MipLevels = {}) const;
	Descriptor GetUnorderedAccessView(RenderResourceHandle Handle, std::optional<UINT> ArraySlice = {}, std::optional<UINT> MipSlice = {}) const;
	Descriptor GetRenderTargetView(RenderResourceHandle Handle, std::optional<UINT> ArraySlice = {}, std::optional<UINT> MipSlice = {}, std::optional<UINT> ArraySize = {}) const;
	Descriptor GetDepthStencilView(RenderResourceHandle Handle, std::optional<UINT> ArraySlice = {}, std::optional<UINT> MipSlice = {}, std::optional<UINT> ArraySize = {}) const;

private:
	void InitializeDXGIObjects();
	void InitializeDXGISwapChain(const Window* pWindow);

	CommandQueue& GetApiCommandQueue(CommandContext::Type Type);
	void AddShaderLayoutRootParameterToBuilder(RootSignatureBuilder& RootSignatureBuilder);

private:
	Microsoft::WRL::ComPtr<IDXGIFactory6>						m_DXGIFactory;
	Microsoft::WRL::ComPtr<IDXGIAdapter4>						m_DXGIAdapter;
	UINT														m_AdapterID;
	std::wstring												m_AdapterDescription;
	bool														m_TearingSupport;
	Microsoft::WRL::ComPtr<IDXGISwapChain4>						m_DXGISwapChain;
	UINT														m_BackBufferIndex;

public:
	Device														Device;
	CommandQueue												GraphicsQueue, ComputeQueue, CopyQueue;
	UINT64														GraphicsFenceValue, ComputeFenceValue, CopyFenceValue;
	Microsoft::WRL::ComPtr<ID3D12Fence1>						GraphicsFence, ComputeFence, CopyFence;
	wil::unique_event											GraphicsFenceCompletionEvent, ComputeFenceCompletionEvent, CopyFenceCompletionEvent;

	ShaderCompiler												ShaderCompiler;

private:
	RenderResourceHandle										m_BackBufferHandle[NumSwapChainBuffers];
	ResourceStateTracker										m_GlobalResourceStateTracker;
	RWLock														m_GlobalResourceStateTrackerRWLock;

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