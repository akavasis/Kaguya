#pragma once
#include <wil/resource.h>
#include <wrl/client.h>
#include <d3d12.h>

#include <memory>
#include <functional>
#include <unordered_map>

#include <Core/Pool.h>

#include "RHI/D3D12/Device.h"
#include "RHI/D3D12/CommandQueue.h"
#include "RHI/D3D12/ResourceStateTracker.h"
#include "RHI/D3D12/ShaderCompiler.h"
#include "RHI/D3D12/DescriptorHeap.h"
#include "RHI/D3D12/CommandList.h"
#include "RHI/D3D12/RaytracingAccelerationStructure.h"

#include "RHI/D3D12/RootSignatureBuilder.h"
#include "RHI/D3D12/PipelineStateBuilder.h"
#include "RHI/D3D12/RaytracingPipelineStateBuilder.h"

struct RootParameters
{
	struct DescriptorTable
	{
		enum
		{
			ShaderResourceDescriptorTable,
			UnorderedAccessDescriptorTable,
			SamplerDescriptorTable,
			NumRootParameters
		};
	};
};

struct Resource
{
	~Resource();

	Microsoft::WRL::ComPtr<ID3D12Resource> pResource;
	D3D12MA::Allocation* pAllocation = nullptr;
};

class RenderDevice
{
public:
	enum
	{
		NumSwapChainBuffers = 3,

		NumConstantBufferDescriptors = 512,
		NumShaderResourceDescriptors = 512,
		NumUnorderedAccessDescriptors = 512,
		NumGlobalOnlineSamplerDescriptors = 512,
		NumRenderTargetDescriptors = 64,
		NumDepthStencilDescriptors = 64
	};

	static constexpr DXGI_FORMAT SwapChainBufferFormat = DXGI_FORMAT_R8G8B8A8_UNORM;
	static constexpr DXGI_FORMAT DepthFormat = DXGI_FORMAT_D32_FLOAT;
	static constexpr DXGI_FORMAT DepthStencilFormat = DXGI_FORMAT_D24_UNORM_S8_UINT;

	RenderDevice(const Window& Window);
	RenderDevice(const RenderDevice&) = delete;
	RenderDevice& operator=(const RenderDevice&) = delete;
	~RenderDevice();

	ID3D12Resource* GetCurrentBackBuffer() const
	{
		return m_SwapChainBuffers[m_BackBufferIndex].Get();
	}

	Descriptor GetCurrentBackBufferRenderTargetView() const
	{
		return SwapChainBufferDescriptors[m_BackBufferIndex];
	}

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

	void BindGlobalDescriptorHeap(CommandList& CommandList);
	void BindDescriptorTable(PipelineState::Type Type, const RootSignature& RootSignature, CommandList& CommandList);

	void ExecuteGraphicsContexts(UINT NumCommandLists, CommandList* ppCommandLists[]) { ExecuteCommandListsInternal(D3D12_COMMAND_LIST_TYPE_DIRECT, NumCommandLists, ppCommandLists); }
	void ExecuteAsyncComputeContexts(UINT NumCommandLists, CommandList* ppCommandLists[]) { ExecuteCommandListsInternal(D3D12_COMMAND_LIST_TYPE_COMPUTE, NumCommandLists, ppCommandLists); }
	void ExecuteCopyContexts(UINT NumCommandLists, CommandList* ppCommandLists[]) { ExecuteCommandListsInternal(D3D12_COMMAND_LIST_TYPE_COPY, NumCommandLists, ppCommandLists); }

	void FlushGraphicsQueue();
	void FlushComputeQueue();
	void FlushCopyQueue();

	// Resource creation
	[[nodiscard]] std::shared_ptr<Resource> CreateResource(const D3D12MA::ALLOCATION_DESC* pAllocDesc,
		const D3D12_RESOURCE_DESC* pResourceDesc,
		D3D12_RESOURCE_STATES InitialResourceState,
		const D3D12_CLEAR_VALUE* pOptimizedClearValue);

	[[nodiscard]] std::shared_ptr<Resource> CreateBuffer(const D3D12MA::ALLOCATION_DESC* pAllocDesc,
		UINT64 Width,
		D3D12_RESOURCE_FLAGS Flags = D3D12_RESOURCE_FLAG_NONE,
		UINT64 Alignment = 0,
		D3D12_RESOURCE_STATES InitialResourceState = D3D12_RESOURCE_STATE_COMMON)
	{
		auto Desc = CD3DX12_RESOURCE_DESC::Buffer(Width, Flags, Alignment);
		return CreateResource(pAllocDesc, &Desc, InitialResourceState, nullptr);
	}

	[[nodiscard]] RootSignature CreateRootSignature(std::function<void(RootSignatureBuilder&)> Configurator, bool AddDescriptorTableRootParameters = true);

	[[nodiscard]] PipelineState CreateGraphicsPipelineState(std::function<void(GraphicsPipelineStateBuilder&)> Configurator);
	[[nodiscard]] PipelineState CreateComputePipelineState(std::function<void(ComputePipelineStateBuilder&)> Configurator);
	[[nodiscard]] RaytracingPipelineState CreateRaytracingPipelineState(std::function<void(RaytracingPipelineStateBuilder&)> Configurator);

	// Resource view creation
	// Thread-Safe
	Descriptor AllocateShaderResourceView();
	Descriptor AllocateUnorderedAccessView();
	Descriptor AllocateRenderTargetView();
	Descriptor AllocateDepthStencilView();

	// Thread-Safe
	void CreateShaderResourceView(ID3D12Resource* pResource,
		const Descriptor& DestDescriptor,
		UINT NumElements,
		UINT Stride,
		bool IsRawBuffer = false);

	template<typename T>
	void CreateShaderResourceView(ID3D12Resource* pResource,
		const Descriptor& DestDescriptor,
		UINT NumElements)
	{
		return CreateShaderResourceView(pResource, DestDescriptor, NumElements, sizeof(T));
	}

	void CreateShaderResourceView(ID3D12Resource* pResource,
		const Descriptor& DestDescriptor,
		std::optional<UINT> MostDetailedMip = {},
		std::optional<UINT> MipLevels = {});

	void CreateUnorderedAccessView(ID3D12Resource* pResource,
		const Descriptor& DestDescriptor,
		std::optional<UINT> ArraySlice = {},
		std::optional<UINT> MipSlice = {});

	void CreateRenderTargetView(ID3D12Resource* pResource,
		const Descriptor& DestDescriptor,
		std::optional<UINT> ArraySlice = {},
		std::optional<UINT> MipSlice = {},
		std::optional<UINT> ArraySize = {});

	void CreateDepthStencilView(ID3D12Resource* pResource,
		const Descriptor& DestDescriptor,
		std::optional<UINT> ArraySlice = {},
		std::optional<UINT> MipSlice = {},
		std::optional<UINT> ArraySize = {});

	void UpdateResourceState(ID3D12Resource* pResource, UINT Subresource, D3D12_RESOURCE_STATES ResourceStates);
private:
	void InitializeDXGIObjects();
	void InitializeDXGISwapChain(const Window& pWindow);

	CommandQueue& GetCommandQueue(D3D12_COMMAND_LIST_TYPE CommandListType);
	void AddDescriptorTableRootParameterToBuilder(RootSignatureBuilder& RootSignatureBuilder);

	void ExecuteCommandListsInternal(D3D12_COMMAND_LIST_TYPE Type, UINT NumCommandLists, CommandList* ppCommandLists[]);
private:
	Microsoft::WRL::ComPtr<IDXGIFactory6>						m_DXGIFactory;
	Microsoft::WRL::ComPtr<IDXGIAdapter4>						m_DXGIAdapter;
	UINT														m_AdapterID;
	DXGI_ADAPTER_DESC3											m_AdapterDesc;
	bool														m_TearingSupport;
	Microsoft::WRL::ComPtr<IDXGISwapChain4>						m_DXGISwapChain;
	UINT														m_BackBufferIndex;

	Microsoft::WRL::ComPtr<ID3D12Resource>						m_SwapChainBuffers[NumSwapChainBuffers];
public:
	Device														Device;
	CommandQueue												GraphicsQueue, ComputeQueue, CopyQueue;
	UINT64														GraphicsFenceValue, ComputeFenceValue, CopyFenceValue;
	Microsoft::WRL::ComPtr<ID3D12Fence>							GraphicsFence, ComputeFence, CopyFence;
	wil::unique_event											GraphicsFenceCompletionEvent, ComputeFenceCompletionEvent, CopyFenceCompletionEvent;

	ShaderCompiler												ShaderCompiler;
private:
	std::vector<CommandList>									m_GraphicsContexts;
	std::vector<CommandList>									m_AsyncComputeContexts;
	std::vector<CommandList>									m_CopyContexts;

	DescriptorHeap												m_GlobalOnlineDescriptorHeap;
	DescriptorHeap												m_GlobalOnlineSamplerDescriptorHeap;
	DescriptorHeap												m_RenderTargetDescriptorHeap;
	DescriptorHeap												m_DepthStencilDescriptorHeap;

	ThreadSafePool<void, NumConstantBufferDescriptors>			m_GlobalOnlineCBDescriptorIndexPool;
	ThreadSafePool<void, NumShaderResourceDescriptors>			m_GlobalOnlineSRDescriptorIndexPool;
	ThreadSafePool<void, NumUnorderedAccessDescriptors>			m_GlobalOnlineUADescriptorIndexPool;
	ThreadSafePool<void, NumGlobalOnlineSamplerDescriptors>		m_GlobalOnlineSamplerDescriptorIndexPool;
	ThreadSafePool<void, NumRenderTargetDescriptors>			m_RenderTargetDescriptorIndexPool;
	ThreadSafePool<void, NumDepthStencilDescriptors>			m_DepthStencilDescriptorIndexPool;

	Descriptor													ImGuiDescriptor;
	Descriptor													SwapChainBufferDescriptors[NumSwapChainBuffers];
};