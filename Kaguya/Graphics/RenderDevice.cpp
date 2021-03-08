#include "pch.h"
#include "RenderDevice.h"

using Microsoft::WRL::ComPtr;

static RenderDevice* s_pRenderDevice = nullptr;

// Global resource state tracker
static RWLock s_ResourceStateTrackerLock;
static ResourceStateTracker s_ResourceStateTracker;

namespace D3D12Utility
{
	void FlushCommandQueue(UINT64 Value, ID3D12Fence* pFence, ID3D12CommandQueue* pCommandQueue, wil::unique_event& Event)
	{
		ThrowIfFailed(pCommandQueue->Signal(pFence, Value));
		ThrowIfFailed(pFence->SetEventOnCompletion(Value, Event.get()));
		Event.wait();
	}
}

Resource::~Resource()
{
	SafeRelease(pAllocation);
}

void RenderDevice::Initialize()
{
	if (!s_pRenderDevice)
	{
		s_pRenderDevice = new RenderDevice();
	}
}

void RenderDevice::Shutdown()
{
	if (s_pRenderDevice)
	{
		s_pRenderDevice->FlushGraphicsQueue();
		s_pRenderDevice->FlushComputeQueue();
		s_pRenderDevice->FlushCopyQueue();
		delete s_pRenderDevice;
	}
	Device::ReportLiveObjects();
}

RenderDevice& RenderDevice::Instance()
{
	assert(s_pRenderDevice);
	return *s_pRenderDevice;
}

RenderDevice::RenderDevice()
{
	InitializeDXGIObjects();

	Device.Create(m_DXGIAdapter.Get());

	GraphicsQueue.Create(Device, D3D12_COMMAND_LIST_TYPE_DIRECT);
	ComputeQueue.Create(Device, D3D12_COMMAND_LIST_TYPE_COMPUTE);
	CopyQueue.Create(Device, D3D12_COMMAND_LIST_TYPE_COPY);

	InitializeDXGISwapChain();

	m_GlobalOnlineDescriptorHeap.Create(Device, { NumConstantBufferDescriptors, NumShaderResourceDescriptors, NumUnorderedAccessDescriptors }, true, D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);
	m_GlobalOnlineSamplerDescriptorHeap.Create(Device, { NumGlobalOnlineSamplerDescriptors }, true, D3D12_DESCRIPTOR_HEAP_TYPE_SAMPLER);
	m_RenderTargetDescriptorHeap.Create(Device, { NumRenderTargetDescriptors }, false, D3D12_DESCRIPTOR_HEAP_TYPE_RTV);
	m_DepthStencilDescriptorHeap.Create(Device, { NumDepthStencilDescriptors }, false, D3D12_DESCRIPTOR_HEAP_TYPE_DSV);

	GraphicsFenceValue = ComputeFenceValue = CopyFenceValue = 1;

	ThrowIfFailed(Device->CreateFence(0, D3D12_FENCE_FLAG_NONE, IID_PPV_ARGS(GraphicsFence.ReleaseAndGetAddressOf())));
	ThrowIfFailed(Device->CreateFence(0, D3D12_FENCE_FLAG_NONE, IID_PPV_ARGS(ComputeFence.ReleaseAndGetAddressOf())));
	ThrowIfFailed(Device->CreateFence(0, D3D12_FENCE_FLAG_NONE, IID_PPV_ARGS(CopyFence.ReleaseAndGetAddressOf())));

	GraphicsFenceCompletionEvent.create();
	ComputeFenceCompletionEvent.create();
	CopyFenceCompletionEvent.create();

	// Allocate RTV for SwapChain
	for (auto& SwapChainDescriptor : SwapChainBufferDescriptors)
	{
		SwapChainDescriptor = AllocateRenderTargetView();
	}

	ImGuiDescriptor = AllocateShaderResourceView();
	// Initialize ImGui for d3d12
	ImGui_ImplDX12_Init(Device, 1,
		RenderDevice::SwapChainBufferFormat, m_GlobalOnlineDescriptorHeap,
		ImGuiDescriptor.CpuHandle,
		ImGuiDescriptor.GpuHandle);
}

RenderDevice::~RenderDevice()
{
	ImGui_ImplDX12_Shutdown();
}

void RenderDevice::CreateCommandContexts(UINT NumGraphicsContext)
{
	// + 1 for Default
	NumGraphicsContext = NumGraphicsContext + 1;
	constexpr UINT NumAsyncComputeContext = 1;
	constexpr UINT NumCopyContext = 1;

	m_GraphicsContexts.reserve(NumGraphicsContext);
	m_AsyncComputeContexts.reserve(NumAsyncComputeContext);
	m_CopyContexts.reserve(NumCopyContext);

	for (UINT i = 0; i < NumGraphicsContext; ++i)
	{
		m_GraphicsContexts.emplace_back(Device, D3D12_COMMAND_LIST_TYPE_DIRECT);
	}

	for (UINT i = 0; i < NumAsyncComputeContext; ++i)
	{
		m_AsyncComputeContexts.emplace_back(Device, D3D12_COMMAND_LIST_TYPE_COMPUTE);
	}

	for (UINT i = 0; i < NumCopyContext; ++i)
	{
		m_CopyContexts.emplace_back(Device, D3D12_COMMAND_LIST_TYPE_COPY);
	}
}

DXGI_QUERY_VIDEO_MEMORY_INFO RenderDevice::QueryLocalVideoMemoryInfo() const
{
	DXGI_QUERY_VIDEO_MEMORY_INFO LocalVideoMemoryInfo = {};
	if (m_DXGIAdapter)
		m_DXGIAdapter->QueryVideoMemoryInfo(0, DXGI_MEMORY_SEGMENT_GROUP_LOCAL, &LocalVideoMemoryInfo);
	return LocalVideoMemoryInfo;
}

void RenderDevice::Present(bool VSync)
{
	UINT	SyncInterval = VSync ? 1u : 0u;
	UINT	PresentFlags = (m_TearingSupport && !VSync) ? DXGI_PRESENT_ALLOW_TEARING : 0u;
	HRESULT hr = m_DXGISwapChain->Present(SyncInterval, PresentFlags);
	if (hr == DXGI_ERROR_DEVICE_REMOVED)
	{
		// TODO: Handle device removal
		LOG_ERROR("DXGI_ERROR_DEVICE_REMOVED");
	}

	m_BackBufferIndex = m_DXGISwapChain->GetCurrentBackBufferIndex();
}

void RenderDevice::Resize(UINT Width, UINT Height)
{
	// Release resources before resize swap chain
	for (auto& SwapChainBuffer : m_SwapChainBuffers)
	{
		SwapChainBuffer.Reset();
	}

	// Resize backbuffer
	// Note: Cannot use ResizeBuffers1 when debugging in Nsight Graphics, it will crash
	DXGI_SWAP_CHAIN_DESC1 Desc = {};
	ThrowIfFailed(m_DXGISwapChain->GetDesc1(&Desc));
	ThrowIfFailed(m_DXGISwapChain->ResizeBuffers(0, Width, Height, DXGI_FORMAT_UNKNOWN, Desc.Flags));

	// Recreate descriptors
	for (uint32_t i = 0; i < RenderDevice::NumSwapChainBuffers; ++i)
	{
		ComPtr<ID3D12Resource> pBackBuffer;
		ThrowIfFailed(m_DXGISwapChain->GetBuffer(i, IID_PPV_ARGS(pBackBuffer.ReleaseAndGetAddressOf())));

		CreateRenderTargetView(pBackBuffer.Get(), SwapChainBufferDescriptors[i]);

		m_SwapChainBuffers[i] = std::move(pBackBuffer);
	}

	ScopedWriteLock SWL(s_ResourceStateTrackerLock);
	for (uint32_t i = 0; i < RenderDevice::NumSwapChainBuffers; ++i)
	{
		s_ResourceStateTracker.AddResourceState(m_SwapChainBuffers[i].Get(), D3D12_RESOURCE_STATE_COMMON);
	}

	// Reset back buffer index
	m_BackBufferIndex = 0;
}

void RenderDevice::BindGlobalDescriptorHeap(CommandList& CommandList)
{
	CommandList.SetDescriptorHeaps(&m_GlobalOnlineDescriptorHeap, &m_GlobalOnlineSamplerDescriptorHeap);
}

void RenderDevice::BindDescriptorTable(PipelineState::Type Type, const RootSignature& RootSignature, CommandList& CommandList)
{
	const auto RootParameterOffset = RootSignature.NumParameters - RootParameters::DescriptorTable::NumRootParameters;
	auto GlobalSRDescriptorFromStart = m_GlobalOnlineDescriptorHeap.GetDescriptorAt(1, 0);
	auto GlobalUADescriptorFromStart = m_GlobalOnlineDescriptorHeap.GetDescriptorAt(2, 0);
	auto GlobalSamplerDescriptorFromStart = m_GlobalOnlineSamplerDescriptorHeap.GetDescriptorFromStart();

	auto Bind = [&](ID3D12GraphicsCommandList* pCommandList, void(ID3D12GraphicsCommandList::* pFunction)(UINT, D3D12_GPU_DESCRIPTOR_HANDLE))
	{
		(pCommandList->*pFunction)(RootParameters::DescriptorTable::ShaderResourceDescriptorTable + RootParameterOffset, GlobalSRDescriptorFromStart.GpuHandle);
		(pCommandList->*pFunction)(RootParameters::DescriptorTable::UnorderedAccessDescriptorTable + RootParameterOffset, GlobalUADescriptorFromStart.GpuHandle);
		(pCommandList->*pFunction)(RootParameters::DescriptorTable::SamplerDescriptorTable + RootParameterOffset, GlobalSamplerDescriptorFromStart.GpuHandle);
	};

	switch (Type)
	{
	case PipelineState::Type::Graphics:
		Bind(CommandList.pCommandList.Get(), &ID3D12GraphicsCommandList::SetGraphicsRootDescriptorTable);
		break;
	case PipelineState::Type::Compute:
		Bind(CommandList.pCommandList.Get(), &ID3D12GraphicsCommandList::SetComputeRootDescriptorTable);
		break;
	default:
		break;
	}
}

void RenderDevice::FlushGraphicsQueue()
{
	UINT64 Value = ++GraphicsFenceValue;
	D3D12Utility::FlushCommandQueue(Value, GraphicsFence.Get(), GraphicsQueue, GraphicsFenceCompletionEvent);
}

void RenderDevice::FlushComputeQueue()
{
	UINT64 Value = ++ComputeFenceValue;
	D3D12Utility::FlushCommandQueue(Value, ComputeFence.Get(), ComputeQueue, ComputeFenceCompletionEvent);
}

void RenderDevice::FlushCopyQueue()
{
	UINT64 Value = ++CopyFenceValue;
	D3D12Utility::FlushCommandQueue(Value, CopyFence.Get(), CopyQueue, CopyFenceCompletionEvent);
}

std::shared_ptr<Resource> RenderDevice::CreateResource(const D3D12MA::ALLOCATION_DESC* pAllocDesc,
	const D3D12_RESOURCE_DESC* pResourceDesc,
	D3D12_RESOURCE_STATES InitialResourceState,
	const D3D12_CLEAR_VALUE* pOptimizedClearValue)
{
	std::shared_ptr<Resource> pResource = std::make_shared<Resource>();

	ThrowIfFailed(Device.Allocator()->CreateResource(pAllocDesc,
		pResourceDesc, InitialResourceState, pOptimizedClearValue,
		&pResource->pAllocation, IID_PPV_ARGS(pResource->pResource.ReleaseAndGetAddressOf())));

	// No need to track resources that have constant resource state throughout their lifetime
	if (pAllocDesc->HeapType == D3D12_HEAP_TYPE_DEFAULT)
	{
		ScopedWriteLock SWL(s_ResourceStateTrackerLock);
		s_ResourceStateTracker.AddResourceState(pResource->pResource.Get(), InitialResourceState);
	}

	return pResource;
}

RootSignature RenderDevice::CreateRootSignature(std::function<void(RootSignatureBuilder&)> Configurator, bool AddDescriptorTableRootParameters)
{
	RootSignatureBuilder Builder = {};
	Configurator(Builder);
	if (AddDescriptorTableRootParameters)
		AddDescriptorTableRootParameterToBuilder(Builder);

	return RootSignature(Device, Builder);
}

PipelineState RenderDevice::CreateGraphicsPipelineState(std::function<void(GraphicsPipelineStateBuilder&)> Configurator)
{
	GraphicsPipelineStateBuilder Builder = {};
	Configurator(Builder);

	return PipelineState(Device, Builder);
}

PipelineState RenderDevice::CreateComputePipelineState(std::function<void(ComputePipelineStateBuilder&)> Configurator)
{
	ComputePipelineStateBuilder Builder = {};
	Configurator(Builder);

	return PipelineState(Device, Builder);
}

RaytracingPipelineState RenderDevice::CreateRaytracingPipelineState(std::function<void(RaytracingPipelineStateBuilder&)> Configurator)
{
	RaytracingPipelineStateBuilder Builder = {};
	Configurator(Builder);

	return RaytracingPipelineState(Device, Builder);
}

Descriptor RenderDevice::AllocateShaderResourceView()
{
	UINT Index = m_GlobalOnlineSRDescriptorIndexPool.Allocate();
	return m_GlobalOnlineDescriptorHeap.GetDescriptorAt(1, Index);
}

Descriptor RenderDevice::AllocateUnorderedAccessView()
{
	UINT Index = m_GlobalOnlineUADescriptorIndexPool.Allocate();
	return m_GlobalOnlineDescriptorHeap.GetDescriptorAt(2, Index);
}

Descriptor RenderDevice::AllocateRenderTargetView()
{
	UINT Index = m_RenderTargetDescriptorIndexPool.Allocate();
	return m_RenderTargetDescriptorHeap.GetDescriptorAt(0, Index);
}

Descriptor RenderDevice::AllocateDepthStencilView()
{
	UINT Index = m_DepthStencilDescriptorIndexPool.Allocate();
	return m_DepthStencilDescriptorHeap.GetDescriptorAt(0, Index);
}

void RenderDevice::CreateShaderResourceView(ID3D12Resource* pResource,
	const Descriptor& DestDescriptor,
	UINT NumElements,
	UINT Stride,
	bool IsRawBuffer /*= false*/)
{
	const auto Desc = pResource->GetDesc();

	D3D12_SHADER_RESOURCE_VIEW_DESC SRVDesc = {};

	switch (Desc.Dimension)
	{
	case D3D12_RESOURCE_DIMENSION_BUFFER:
		SRVDesc.Format = DXGI_FORMAT_UNKNOWN;
		SRVDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
		SRVDesc.ViewDimension = D3D12_SRV_DIMENSION_BUFFER;
		SRVDesc.Buffer.FirstElement = 0;
		SRVDesc.Buffer.NumElements = NumElements;
		SRVDesc.Buffer.StructureByteStride = Stride;
		SRVDesc.Buffer.Flags = IsRawBuffer ? D3D12_BUFFER_SRV_FLAG_NONE : D3D12_BUFFER_SRV_FLAG_RAW;
		break;

	default:
		break;
	}

	Device->CreateShaderResourceView(pResource, &SRVDesc, DestDescriptor.CpuHandle);
}

void RenderDevice::CreateShaderResourceView(ID3D12Resource* pResource,
	const Descriptor& DestDescriptor,
	std::optional<UINT> MostDetailedMip /*= {}*/,
	std::optional<UINT> MipLevels /*= {}*/)
{
	const auto Desc = pResource->GetDesc();

	UINT mostDetailedMip = MostDetailedMip.value_or(0);
	UINT mipLevels = MipLevels.value_or(Desc.MipLevels);

	auto GetValidSRVFormat = [](DXGI_FORMAT Format)
	{
		switch (Format)
		{
		case DXGI_FORMAT_R32_TYPELESS:		return DXGI_FORMAT_R32_FLOAT;
		case DXGI_FORMAT_D24_UNORM_S8_UINT: return DXGI_FORMAT_R24_UNORM_X8_TYPELESS;
		case DXGI_FORMAT_D32_FLOAT:			return DXGI_FORMAT_R32_FLOAT;
		default:							return Format;
		}
	};

	D3D12_SHADER_RESOURCE_VIEW_DESC SRVDesc = {};
	SRVDesc.Format = GetValidSRVFormat(Desc.Format);
	SRVDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;

	switch (Desc.Dimension)
	{
	case D3D12_RESOURCE_DIMENSION_TEXTURE2D:
		if (Desc.DepthOrArraySize > 1)
		{
			SRVDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2DARRAY;
			SRVDesc.Texture2DArray.MostDetailedMip = mostDetailedMip;
			SRVDesc.Texture2DArray.MipLevels = mipLevels;
			SRVDesc.Texture2DArray.ArraySize = Desc.DepthOrArraySize;
			SRVDesc.Texture2DArray.PlaneSlice = 0;
			SRVDesc.Texture2DArray.ResourceMinLODClamp = 0.0f;
		}
		else
		{
			SRVDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D;
			SRVDesc.Texture2D.MostDetailedMip = mostDetailedMip;
			SRVDesc.Texture2D.MipLevels = mipLevels;
			SRVDesc.Texture2D.PlaneSlice = 0;
			SRVDesc.Texture2D.ResourceMinLODClamp = 0.0f;
		}
		break;

	default:
		break;
	}

	Device->CreateShaderResourceView(pResource, &SRVDesc, DestDescriptor.CpuHandle);
}

void RenderDevice::CreateUnorderedAccessView(ID3D12Resource* pResource,
	const Descriptor& DestDescriptor,
	std::optional<UINT> ArraySlice /*= {}*/,
	std::optional<UINT> MipSlice /*= {}*/)
{
	const auto Desc = pResource->GetDesc();

	UINT arraySlice = ArraySlice.value_or(0);
	UINT mipSlice = MipSlice.value_or(0);

	D3D12_UNORDERED_ACCESS_VIEW_DESC UAVDesc = {};
	UAVDesc.Format = Desc.Format;

	switch (Desc.Dimension)
	{
	case D3D12_RESOURCE_DIMENSION_TEXTURE2D:
		if (Desc.DepthOrArraySize > 1)
		{
			UAVDesc.ViewDimension = D3D12_UAV_DIMENSION_TEXTURE2DARRAY;
			UAVDesc.Texture2DArray.MipSlice = mipSlice;
			UAVDesc.Texture2DArray.FirstArraySlice = arraySlice;
			UAVDesc.Texture2DArray.ArraySize = Desc.DepthOrArraySize;
			UAVDesc.Texture2DArray.PlaneSlice = 0;
		}
		else
		{
			UAVDesc.ViewDimension = D3D12_UAV_DIMENSION_TEXTURE2D;
			UAVDesc.Texture2D.MipSlice = mipSlice;
			UAVDesc.Texture2D.PlaneSlice = 0;
		}
		break;

	default:
		break;
	}

	Device->CreateUnorderedAccessView(pResource, nullptr, &UAVDesc, DestDescriptor.CpuHandle);
}

void RenderDevice::CreateRenderTargetView(ID3D12Resource* pResource,
	const Descriptor& DestDescriptor,
	std::optional<UINT> ArraySlice /*= {}*/,
	std::optional<UINT> MipSlice /*= {}*/,
	std::optional<UINT> ArraySize /*= {}*/,
	bool sRGB /*= false*/)
{
	const auto Desc = pResource->GetDesc();

	UINT arraySlice = ArraySlice.value_or(0);
	UINT mipSlice = MipSlice.value_or(0);
	UINT arraySize = ArraySize.value_or(Desc.DepthOrArraySize);

	D3D12_RENDER_TARGET_VIEW_DESC RTVDesc = {};
	RTVDesc.Format = sRGB ? DirectX::MakeSRGB(Desc.Format) : Desc.Format;

	// TODO: Add 1D/3D support
	switch (Desc.Dimension)
	{
	case D3D12_RESOURCE_DIMENSION_TEXTURE2D:
		if (Desc.DepthOrArraySize > 1)
		{
			RTVDesc.ViewDimension = D3D12_RTV_DIMENSION_TEXTURE2DARRAY;
			RTVDesc.Texture2DArray.MipSlice = mipSlice;
			RTVDesc.Texture2DArray.FirstArraySlice = arraySlice;
			RTVDesc.Texture2DArray.ArraySize = arraySize;
			RTVDesc.Texture2DArray.PlaneSlice = 0;
		}
		else
		{
			RTVDesc.ViewDimension = D3D12_RTV_DIMENSION_TEXTURE2D;
			RTVDesc.Texture2D.MipSlice = mipSlice;
			RTVDesc.Texture2D.PlaneSlice = 0;
		}
		break;

	default:
		break;
	}

	Device->CreateRenderTargetView(pResource, &RTVDesc, DestDescriptor.CpuHandle);
}

void RenderDevice::CreateDepthStencilView(ID3D12Resource* pResource,
	const Descriptor& DestDescriptor,
	std::optional<UINT> ArraySlice /*= {}*/,
	std::optional<UINT> MipSlice /*= {}*/,
	std::optional<UINT> ArraySize /*= {}*/)
{
	const auto Desc = pResource->GetDesc();

	UINT arraySlice = ArraySlice.value_or(0);
	UINT mipSlice = MipSlice.value_or(0);
	UINT arraySize = ArraySize.value_or(Desc.DepthOrArraySize);

	auto GetValidDSVFormat = [](DXGI_FORMAT Format)
	{
		// TODO: Add more
		switch (Format)
		{
		case DXGI_FORMAT_R32_TYPELESS: return DXGI_FORMAT_D32_FLOAT;

		default: return DXGI_FORMAT_UNKNOWN;
		}
	};

	D3D12_DEPTH_STENCIL_VIEW_DESC DSVDesc = {};
	DSVDesc.Format = GetValidDSVFormat(Desc.Format);
	DSVDesc.Flags = D3D12_DSV_FLAG_NONE;

	switch (Desc.Dimension)
	{
	case D3D12_RESOURCE_DIMENSION_TEXTURE2D:
		if (Desc.DepthOrArraySize > 1)
		{
			DSVDesc.ViewDimension = D3D12_DSV_DIMENSION_TEXTURE2DARRAY;
			DSVDesc.Texture2DArray.MipSlice = mipSlice;
			DSVDesc.Texture2DArray.FirstArraySlice = arraySlice;
			DSVDesc.Texture2DArray.ArraySize = arraySize;
		}
		else
		{
			DSVDesc.ViewDimension = D3D12_DSV_DIMENSION_TEXTURE2D;
			DSVDesc.Texture2D.MipSlice = mipSlice;
		}
		break;

	case D3D12_RESOURCE_DIMENSION_TEXTURE3D:
		throw std::invalid_argument("Invalid D3D12_RESOURCE_DIMENSION. Dimension: D3D12_RESOURCE_DIMENSION_TEXTURE3D");

	default:
		break;
	}

	Device->CreateDepthStencilView(pResource, &DSVDesc, DestDescriptor.CpuHandle);
}

void RenderDevice::UpdateResourceState(ID3D12Resource* pResource, UINT Subresource, D3D12_RESOURCE_STATES ResourceStates)
{
	if (!pResource)
	{
		return;
	}

	ScopedWriteLock SWL(s_ResourceStateTrackerLock);
	s_ResourceStateTracker.SetResourceState(pResource, Subresource, ResourceStates);
}

void RenderDevice::InitializeDXGIObjects()
{
	// Create DXGIFactory
	UINT FactoryFlags = 0;
#if defined (_DEBUG)
	FactoryFlags |= DXGI_CREATE_FACTORY_DEBUG;
#endif
	ThrowIfFailed(::CreateDXGIFactory2(FactoryFlags, IID_PPV_ARGS(m_DXGIFactory.ReleaseAndGetAddressOf())));

	// Check tearing support
	BOOL AllowTearing = FALSE;
	if (FAILED(m_DXGIFactory->CheckFeatureSupport(
		DXGI_FEATURE_PRESENT_ALLOW_TEARING,
		&AllowTearing, sizeof(AllowTearing))))
	{
		AllowTearing = FALSE;
	}
	m_TearingSupport = AllowTearing == TRUE;

	// Enumerate hardware for an adapter that supports DX12
	ComPtr<IDXGIAdapter4> pAdapter4;
	UINT AdapterID = 0;
	while (m_DXGIFactory->EnumAdapterByGpuPreference(AdapterID, DXGI_GPU_PREFERENCE_HIGH_PERFORMANCE, IID_PPV_ARGS(pAdapter4.ReleaseAndGetAddressOf())) != DXGI_ERROR_NOT_FOUND)
	{
		DXGI_ADAPTER_DESC3 Desc = {};
		ThrowIfFailed(pAdapter4->GetDesc3(&Desc));

		if ((Desc.Flags & DXGI_ADAPTER_FLAG_SOFTWARE))
		{
			// Skip SOFTWARE adapters
			continue;
		}

		if (SUCCEEDED(::D3D12CreateDevice(pAdapter4.Get(), D3D_FEATURE_LEVEL_11_0, __uuidof(ID3D12Device), nullptr)))
		{
			m_DXGIAdapter = pAdapter4;
			m_AdapterID = AdapterID;
			m_AdapterDesc = Desc;
			break;
		}

		AdapterID++;
	}
}

void RenderDevice::InitializeDXGISwapChain()
{
	const Window& Window = Application::Window;

	// Create DXGISwapChain
	DXGI_SWAP_CHAIN_DESC1 Desc = {};
	Desc.Width = Window.GetWindowWidth();
	Desc.Height = Window.GetWindowHeight();
	Desc.Format = SwapChainBufferFormat;
	Desc.Stereo = FALSE;
	Desc.SampleDesc = DefaultSampleDesc();
	Desc.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
	Desc.BufferCount = NumSwapChainBuffers;
	Desc.Scaling = DXGI_SCALING_NONE;
	Desc.SwapEffect = DXGI_SWAP_EFFECT_FLIP_SEQUENTIAL;
	Desc.AlphaMode = DXGI_ALPHA_MODE_UNSPECIFIED;
	Desc.Flags = m_TearingSupport ? DXGI_SWAP_CHAIN_FLAG_ALLOW_TEARING : DXGI_SWAP_CHAIN_FLAG_ALLOW_MODE_SWITCH;
	ComPtr<IDXGISwapChain1> pSwapChain1;
	ThrowIfFailed(m_DXGIFactory->CreateSwapChainForHwnd(GraphicsQueue, Window.GetWindowHandle(), &Desc, nullptr, nullptr, pSwapChain1.ReleaseAndGetAddressOf()));
	ThrowIfFailed(m_DXGIFactory->MakeWindowAssociation(Window.GetWindowHandle(), DXGI_MWA_NO_ALT_ENTER)); // No full screen via alt + enter
	ThrowIfFailed(pSwapChain1->QueryInterface(IID_PPV_ARGS(m_DXGISwapChain.ReleaseAndGetAddressOf())));

	m_BackBufferIndex = m_DXGISwapChain->GetCurrentBackBufferIndex();
}

CommandQueue& RenderDevice::GetCommandQueue(D3D12_COMMAND_LIST_TYPE CommandListType)
{
	switch (CommandListType)
	{
	case D3D12_COMMAND_LIST_TYPE_DIRECT:	return GraphicsQueue;
	case D3D12_COMMAND_LIST_TYPE_COMPUTE:	return ComputeQueue;
	case D3D12_COMMAND_LIST_TYPE_COPY:		return CopyQueue;
	default:								return GraphicsQueue;
	}
}

void RenderDevice::AddDescriptorTableRootParameterToBuilder(RootSignatureBuilder& RootSignatureBuilder)
{
	/* Descriptor Tables */

	// ShaderResource
	RootDescriptorTable ShaderResourceDescriptorTable;
	{
		constexpr D3D12_DESCRIPTOR_RANGE_FLAGS Flags = D3D12_DESCRIPTOR_RANGE_FLAG_DESCRIPTORS_VOLATILE | D3D12_DESCRIPTOR_RANGE_FLAG_DATA_STATIC_WHILE_SET_AT_EXECUTE;

		ShaderResourceDescriptorTable.AddDescriptorRange(DescriptorRange::Type::SRV, DescriptorRange(RootSignature::UnboundDescriptorSize, 0, 100, Flags, 0)); // g_Texture2DTable
		ShaderResourceDescriptorTable.AddDescriptorRange(DescriptorRange::Type::SRV, DescriptorRange(RootSignature::UnboundDescriptorSize, 0, 101, Flags, 0)); // g_Texture2DUINT4Table
		ShaderResourceDescriptorTable.AddDescriptorRange(DescriptorRange::Type::SRV, DescriptorRange(RootSignature::UnboundDescriptorSize, 0, 102, Flags, 0)); // g_Texture2DArrayTable
		ShaderResourceDescriptorTable.AddDescriptorRange(DescriptorRange::Type::SRV, DescriptorRange(RootSignature::UnboundDescriptorSize, 0, 103, Flags, 0)); // g_TextureCubeTable
		ShaderResourceDescriptorTable.AddDescriptorRange(DescriptorRange::Type::SRV, DescriptorRange(RootSignature::UnboundDescriptorSize, 0, 104, Flags, 0)); // g_ByteAddressBufferTable
	}
	RootSignatureBuilder.AddRootDescriptorTableParameter(ShaderResourceDescriptorTable);

	// UnorderedAccess
	RootDescriptorTable UnorderedAccessDescriptorTable;
	{
		constexpr D3D12_DESCRIPTOR_RANGE_FLAGS Flags = D3D12_DESCRIPTOR_RANGE_FLAG_DESCRIPTORS_VOLATILE | D3D12_DESCRIPTOR_RANGE_FLAG_DATA_STATIC_WHILE_SET_AT_EXECUTE;

		UnorderedAccessDescriptorTable.AddDescriptorRange(DescriptorRange::Type::UAV, DescriptorRange(RootSignature::UnboundDescriptorSize, 0, 100, Flags, 0)); // g_RWTexture2DTable
		UnorderedAccessDescriptorTable.AddDescriptorRange(DescriptorRange::Type::UAV, DescriptorRange(RootSignature::UnboundDescriptorSize, 0, 101, Flags, 0)); // g_RWTexture2DArrayTable
		UnorderedAccessDescriptorTable.AddDescriptorRange(DescriptorRange::Type::UAV, DescriptorRange(RootSignature::UnboundDescriptorSize, 0, 102, Flags, 0)); // g_RWByteAddressBufferTable
	}
	RootSignatureBuilder.AddRootDescriptorTableParameter(UnorderedAccessDescriptorTable);

	// Sampler
	RootDescriptorTable SamplerDescriptorTable;
	{
		constexpr D3D12_DESCRIPTOR_RANGE_FLAGS Flags = D3D12_DESCRIPTOR_RANGE_FLAG_DESCRIPTORS_VOLATILE;

		SamplerDescriptorTable.AddDescriptorRange(DescriptorRange::Type::Sampler, DescriptorRange(RootSignature::UnboundDescriptorSize, 0, 100, Flags, 0)); // g_SamplerTable
	}
	RootSignatureBuilder.AddRootDescriptorTableParameter(SamplerDescriptorTable);
}

void RenderDevice::ExecuteCommandListsInternal(D3D12_COMMAND_LIST_TYPE Type, UINT NumCommandLists, CommandList* ppCommandLists[])
{
	ScopedWriteLock SWL(s_ResourceStateTrackerLock);

	std::vector<ID3D12CommandList*> commandlistsToBeExecuted;
	commandlistsToBeExecuted.reserve(size_t(NumCommandLists) * 2);
	for (UINT i = 0; i < NumCommandLists; ++i)
	{
		CommandList* pCommandList = ppCommandLists[i];
		if (pCommandList->Close(&s_ResourceStateTracker))
		{
			commandlistsToBeExecuted.push_back(pCommandList->pPendingCommandList.Get());
		}
		commandlistsToBeExecuted.push_back(pCommandList->pCommandList.Get());
	}

	CommandQueue& CommandQueue = GetCommandQueue(Type);
	CommandQueue->ExecuteCommandLists(commandlistsToBeExecuted.size(), commandlistsToBeExecuted.data());
}