#pragma once
#include <wil/resource.h>
#include <wrl/client.h>
#include <d3d12.h>

#include <memory>
#include <functional>
#include <unordered_map>

#include <Template/Pool.h>

#include "RenderResourceHandle.h"
#include "RenderResourceHandleRegistry.h"
#include "RenderResourceContainer.h"

#include "API/D3D12/Device.h"
#include "API/D3D12/CommandQueue.h"
#include "API/D3D12/ResourceStateTracker.h"
#include "API/D3D12/ShaderCompiler.h"
#include "API/D3D12/DescriptorHeap.h"
#include "API/D3D12/CommandList.h"
#include "API/D3D12/RaytracingAccelerationStructure.h"

#include "API/Proxy/BufferProxy.h"
#include "API/Proxy/TextureProxy.h"

#include "API/D3D12/Heap.h"

#include "API/D3D12/RootSignatureBuilder.h"
#include "API/D3D12/PipelineStateBuilder.h"
#include "API/D3D12/RaytracingPipelineStateBuilder.h"

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

struct RenderBuffer
{
	Buffer* pBuffer;
	Descriptor ShaderResourceView;
	Descriptor UnorderedAccessView;
};

struct RenderTexture
{
	Texture* pTexture;
	std::unordered_map<UINT64, Descriptor> ShaderResourceViews;
	std::unordered_map<UINT64, Descriptor> UnorderedAccessViews;
	std::unordered_map<UINT64, Descriptor> RenderTargetViews;
	std::unordered_map<UINT64, Descriptor> DepthStencilViews;
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

	RenderDevice(const Window& pWindow);
	~RenderDevice();

	void CreateCommandContexts(UINT NumGraphicsContext);

	const auto& GetAdapterDesc() const { return m_AdapterDesc; }
	DXGI_QUERY_VIDEO_MEMORY_INFO QueryLocalVideoMemoryInfo() const;

	void Present(bool VSync);

	void Resize(UINT Width, UINT Height);

	CommandList& GetGraphicsContext(UINT Index) { return m_GraphicsContexts[Index]; }
	CommandList& GetAsyncComputeContext(UINT Index) { return m_AsyncComputeContexts[Index]; }
	CommandList& GetCopyContext(UINT Index) { return m_CopyContexts[Index]; }

	CommandList& GetDefaultGraphicsContext() { return GetGraphicsContext(0); }
	CommandList& GetDefaultAsyncComputeContext() { return GetAsyncComputeContext(0); }
	CommandList& GetDefaultCopyContext() { return GetCopyContext(0); }

	void BindGpuDescriptorHeap(CommandList& CommandList);
	inline auto GetGpuDescriptorHeapCBDescriptorFromStart() const { return m_ShaderVisibleCBSRUADescriptorHeap.GetCBDescriptorAt(0); }
	inline auto GetGpuDescriptorHeapSRDescriptorFromStart() const { return m_ShaderVisibleCBSRUADescriptorHeap.GetSRDescriptorAt(0); }
	inline auto GetGpuDescriptorHeapUADescriptorFromStart() const { return m_ShaderVisibleCBSRUADescriptorHeap.GetUADescriptorAt(0); }
	inline auto GetSamplerDescriptorHeapDescriptorFromStart() const { return m_SamplerDescriptorHeap.GetDescriptorFromStart(); }

	void ExecuteGraphicsContexts(UINT NumCommandLists, CommandList* ppCommandLists[]) { ExecuteCommandListsInternal(D3D12_COMMAND_LIST_TYPE_DIRECT, NumCommandLists, ppCommandLists); }
	void ExecuteAsyncComputeContexts(UINT NumCommandLists, CommandList* ppCommandLists[]) { ExecuteCommandListsInternal(D3D12_COMMAND_LIST_TYPE_COMPUTE, NumCommandLists, ppCommandLists); }
	void ExecuteCopyContexts(UINT NumCommandLists, CommandList* ppCommandLists[]) { ExecuteCommandListsInternal(D3D12_COMMAND_LIST_TYPE_COPY, NumCommandLists, ppCommandLists); }

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
	
	void CreateGraphicsPipelineState(RenderResourceHandle Handle, std::function<void(GraphicsPipelineStateBuilder&)> Configurator);
	void CreateComputePipelineState(RenderResourceHandle Handle, std::function<void(ComputePipelineStateBuilder&)> Configurator);
	void CreateRaytracingPipelineState(RenderResourceHandle Handle, std::function<void(RaytracingPipelineStateBuilder&)> Configurator);

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
	[[nodiscard]] inline auto GetPipelineState(RenderResourceHandle Handle)	{ return m_PipelineStates.GetResource(Handle); }
	[[nodiscard]] inline auto GetRaytracingPipelineState(RenderResourceHandle Handle)	{ return m_RaytracingPipelineStates.GetResource(Handle); }

	Descriptor GetShaderResourceView(RenderResourceHandle Handle, std::optional<UINT> MostDetailedMip = {}, std::optional<UINT> MipLevels = {}) const;
	Descriptor GetUnorderedAccessView(RenderResourceHandle Handle, std::optional<UINT> ArraySlice = {}, std::optional<UINT> MipSlice = {}) const;
	Descriptor GetRenderTargetView(RenderResourceHandle Handle, std::optional<UINT> ArraySlice = {}, std::optional<UINT> MipSlice = {}, std::optional<UINT> ArraySize = {}) const;
	Descriptor GetDepthStencilView(RenderResourceHandle Handle, std::optional<UINT> ArraySlice = {}, std::optional<UINT> MipSlice = {}, std::optional<UINT> ArraySize = {}) const;
private:
	void InitializeDXGIObjects();
	void InitializeDXGISwapChain(const Window& pWindow);

	CommandQueue& GetCommandQueue(D3D12_COMMAND_LIST_TYPE CommandListType);
	void AddShaderLayoutRootParameterToBuilder(RootSignatureBuilder& RootSignatureBuilder);

	void ExecuteCommandListsInternal(D3D12_COMMAND_LIST_TYPE Type, UINT NumCommandLists, CommandList* ppCommandLists[]);
private:
	Microsoft::WRL::ComPtr<IDXGIFactory6>						m_DXGIFactory;
	Microsoft::WRL::ComPtr<IDXGIAdapter4>						m_DXGIAdapter;
	UINT														m_AdapterID;
	DXGI_ADAPTER_DESC3											m_AdapterDesc;
	bool														m_TearingSupport;
	Microsoft::WRL::ComPtr<IDXGISwapChain4>						m_DXGISwapChain;
	UINT														m_BackBufferIndex;

public:
	Device														Device;
	CommandQueue												GraphicsQueue, ComputeQueue, CopyQueue;
	UINT64														GraphicsFenceValue, ComputeFenceValue, CopyFenceValue;
	Microsoft::WRL::ComPtr<ID3D12Fence>							GraphicsFence, ComputeFence, CopyFence;
	wil::unique_event											GraphicsFenceCompletionEvent, ComputeFenceCompletionEvent, CopyFenceCompletionEvent;

	ShaderCompiler												ShaderCompiler;

private:
	RenderResourceHandle										m_BackBufferHandle[NumSwapChainBuffers];
	ResourceStateTracker										m_GlobalResourceStateTracker;
	RWLock														m_GlobalResourceStateTrackerRWLock;

	std::vector<CommandList>									m_GraphicsContexts;
	std::vector<CommandList>									m_AsyncComputeContexts;
	std::vector<CommandList>									m_CopyContexts;

	RenderResourceHandleRegistry								m_BufferHandleRegistry;
	RenderResourceHandleRegistry								m_TextureHandleRegistry;
	RenderResourceHandleRegistry								m_HeapHandleRegistry;
	RenderResourceHandleRegistry								m_RootSignatureHandleRegistry;
	RenderResourceHandleRegistry								m_PipelineStateHandleRegistry;
	RenderResourceHandleRegistry								m_RaytracingPipelineStateHandleRegistry;

	RenderResourceContainer<Buffer>								m_Buffers;
	RenderResourceContainer<Texture>							m_Textures;
	RenderResourceContainer<Heap>								m_Heaps;
	RenderResourceContainer<RootSignature>						m_RootSignatures;
	RenderResourceContainer<PipelineState>						m_PipelineStates;
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

	// Resource descriptor containers
	std::unordered_map<RenderResourceHandle, RenderBuffer>		m_RenderBuffers;
	std::unordered_map<RenderResourceHandle, RenderTexture>		m_RenderTextures;
};