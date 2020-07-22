#include "pch.h"
#include "RenderDevice.h"

RenderDevice::RenderDevice(IDXGIAdapter4* pAdapter)
	: m_Device(pAdapter),
	m_GraphicsQueue(&m_Device, D3D12_COMMAND_LIST_TYPE_DIRECT),
	m_ComputeQueue(&m_Device, D3D12_COMMAND_LIST_TYPE_COMPUTE),
	m_CopyQueue(&m_Device, D3D12_COMMAND_LIST_TYPE_COPY),
	m_DescriptorAllocator(&m_Device)
{
	for (std::size_t i = 0; i < DescriptorRanges::NumDescriptorRanges; ++i)
	{
		m_StandardShaderLayoutDescriptorRanges[i].RangeType = D3D12_DESCRIPTOR_RANGE_TYPE_SRV;
		m_StandardShaderLayoutDescriptorRanges[i].NumDescriptors = UINT_MAX;
		m_StandardShaderLayoutDescriptorRanges[i].BaseShaderRegister = 0;
		m_StandardShaderLayoutDescriptorRanges[i].RegisterSpace = i + 100; // Descriptor range starts at 100 in HLSL
		m_StandardShaderLayoutDescriptorRanges[i].OffsetInDescriptorsFromTableStart = 0;
		m_StandardShaderLayoutDescriptorRanges[i].Flags = D3D12_DESCRIPTOR_RANGE_FLAG_DESCRIPTORS_VOLATILE;
	}
}

RenderDevice::~RenderDevice()
{
}

RenderCommandContext* RenderDevice::AllocateContext(D3D12_COMMAND_LIST_TYPE Type)
{
	m_RenderCommandContexts[Type].push_back(std::make_unique<RenderCommandContext>(this, Type));
	return m_RenderCommandContexts[Type].back().get();
}

void RenderDevice::BindUniversalGpuDescriptorHeap(RenderCommandContext* pRenderCommandContext) const
{
	pRenderCommandContext->SetDescriptorHeaps(m_DescriptorAllocator.GetUniversalGpuDescriptorHeap(), nullptr);
}

void RenderDevice::ExecuteRenderCommandContexts(UINT NumRenderCommandContexts, RenderCommandContext* ppRenderCommandContexts[])
{
	std::vector<ID3D12CommandList*> commandlistsToBeExecuted;
	commandlistsToBeExecuted.reserve(NumRenderCommandContexts);
	for (UINT i = 0; i < NumRenderCommandContexts; ++i)
	{
		RenderCommandContext* pRenderCommandContext = ppRenderCommandContexts[i];
		if (pRenderCommandContext->Close(m_GlobalResourceStateTracker))
		{
			commandlistsToBeExecuted.push_back(pRenderCommandContext->m_pPendingCommandList.Get());
		}
		commandlistsToBeExecuted.push_back(pRenderCommandContext->m_pCommandList.Get());
	}

	m_GraphicsQueue.GetD3DCommandQueue()->ExecuteCommandLists(commandlistsToBeExecuted.size(), commandlistsToBeExecuted.data());
	UINT64 fenceValue = m_GraphicsQueue.Signal();
	for (UINT i = 0; i < NumRenderCommandContexts; ++i)
	{
		RenderCommandContext* pRenderCommandContext = ppRenderCommandContexts[i];
		pRenderCommandContext->RequestNewAllocator(fenceValue);
		pRenderCommandContext->Reset();
	}
	m_GraphicsQueue.Flush();
}

RenderResourceHandle RenderDevice::LoadShader(Shader::Type Type, LPCWSTR pPath, LPCWSTR pEntryPoint, const std::vector<DxcDefine>& ShaderDefines)
{
	auto [handle, shader] = m_Shaders.CreateResource(m_ShaderCompiler.LoadShader(Type, pPath, pEntryPoint, ShaderDefines));
	return handle;
}

RenderResourceHandle RenderDevice::CreateBuffer(const Buffer::Properties& Properties)
{
	auto [handle, buffer] = m_Buffers.CreateResource(&m_Device, Properties);
	m_GlobalResourceStateTracker.AddResourceState(buffer->GetD3DResource(), Properties.InitialState);
	return handle;
}

RenderResourceHandle RenderDevice::CreateBuffer(const Buffer::Properties& Properties, CPUAccessibleHeapType CPUAccessibleHeapType, const void* pData)
{
	auto [handle, buffer] = m_Buffers.CreateResource(&m_Device, Properties, CPUAccessibleHeapType, pData);
	switch (CPUAccessibleHeapType)
	{
	case CPUAccessibleHeapType::Upload:
		m_GlobalResourceStateTracker.AddResourceState(buffer->GetD3DResource(), D3D12_RESOURCE_STATE_GENERIC_READ);
		break;
	case CPUAccessibleHeapType::Readback:
		m_GlobalResourceStateTracker.AddResourceState(buffer->GetD3DResource(), D3D12_RESOURCE_STATE_COPY_DEST);
		break;
	}
	return handle;
}

RenderResourceHandle RenderDevice::CreateBuffer(const Buffer::Properties& Properties, RenderResourceHandle HeapHandle, UINT64 HeapOffset)
{
	const auto pHeap = this->GetHeap(HeapHandle);
	auto [handle, buffer] = m_Buffers.CreateResource(&m_Device, Properties, pHeap, HeapOffset);
	if (pHeap->GetCPUAccessibleHeapType())
	{
		switch (pHeap->GetCPUAccessibleHeapType().value())
		{
		case CPUAccessibleHeapType::Upload:
			m_GlobalResourceStateTracker.AddResourceState(buffer->GetD3DResource(), D3D12_RESOURCE_STATE_GENERIC_READ);
			break;
		case CPUAccessibleHeapType::Readback:
			m_GlobalResourceStateTracker.AddResourceState(buffer->GetD3DResource(), D3D12_RESOURCE_STATE_COPY_DEST);
			break;
		}
	}
	else
	{
		m_GlobalResourceStateTracker.AddResourceState(buffer->GetD3DResource(), Properties.InitialState);
	}
	return handle;
}

RenderResourceHandle RenderDevice::CreateTexture(const Texture::Properties& Properties)
{
	auto [handle, texture] = m_Textures.CreateResource(&m_Device, Properties);
	m_GlobalResourceStateTracker.AddResourceState(texture->GetD3DResource(), Properties.InitialState);
	return handle;
}

RenderResourceHandle RenderDevice::CreateTexture(const Texture::Properties& Properties, RenderResourceHandle HeapHandle, UINT64 HeapOffset)
{
	const auto pHeap = this->GetHeap(HeapHandle);
	auto [handle, texture] = m_Textures.CreateResource(&m_Device, Properties, pHeap, HeapOffset);
	m_GlobalResourceStateTracker.AddResourceState(texture->GetD3DResource(), Properties.InitialState);
	return handle;
}

RenderResourceHandle RenderDevice::CreateHeap(const Heap::Properties& Properties)
{
	auto [handle, heap] = m_Heaps.CreateResource(&m_Device, Properties);
	return handle;
}

RenderResourceHandle RenderDevice::CreateRootSignature(StandardShaderLayoutOptions* pOptions, Delegate<void(RootSignatureProxy&)> Configurator)
{
	RootSignatureProxy Proxy;
	Configurator(Proxy);
	if (pOptions)
		AddStandardShaderLayoutRootParameter(pOptions, Proxy);

	auto [handle, rootSignature] = m_RootSignatures.CreateResource(&m_Device, Proxy);
	return handle;
}

RenderResourceHandle RenderDevice::CreateGraphicsPipelineState(const GraphicsPipelineState::Properties& Properties)
{
	auto [handle, graphicsPSO] = m_GraphicsPipelineStates.CreateResource(&m_Device, Properties);
	return handle;
}

RenderResourceHandle RenderDevice::CreateComputePipelineState(const ComputePipelineState::Properties& Properties)
{
	auto [handle, computePSO] = m_ComputePipelineStates.CreateResource(&m_Device, Properties);
	return handle;
}

void RenderDevice::ReplaceShader(Shader::Type Type, LPCWSTR pPath, LPCWSTR pEntryPoint, const std::vector<DxcDefine>& ShaderDefines, RenderResourceHandle ExistingRenderResourceHandle)
{
	auto [shader, success] = m_Shaders.ReplaceResource(ExistingRenderResourceHandle, m_ShaderCompiler.LoadShader(Type, pPath, pEntryPoint, ShaderDefines));
	if (success)
	{
		CORE_INFO("Shader replaced");
	}
}

void RenderDevice::ReplaceBuffer(const Buffer::Properties& Properties, RenderResourceHandle ExistingRenderResourceHandle)
{
	auto [buffer, success] = m_Buffers.ReplaceResource(ExistingRenderResourceHandle, &m_Device, Properties);
	if (success)
	{
		m_GlobalResourceStateTracker.AddResourceState(buffer->GetD3DResource(), Properties.InitialState);
		CORE_INFO("Buffer replaced");
	}
}

void RenderDevice::ReplaceBuffer(const Buffer::Properties& Properties, CPUAccessibleHeapType CPUAccessibleHeapType, const void* pData, RenderResourceHandle ExistingRenderResourceHandle)
{
	auto [buffer, success] = m_Buffers.ReplaceResource(ExistingRenderResourceHandle, &m_Device, Properties, CPUAccessibleHeapType, pData);
	if (success)
	{
		switch (CPUAccessibleHeapType)
		{
		case CPUAccessibleHeapType::Upload:
			m_GlobalResourceStateTracker.AddResourceState(buffer->GetD3DResource(), D3D12_RESOURCE_STATE_GENERIC_READ);
			break;
		case CPUAccessibleHeapType::Readback:
			m_GlobalResourceStateTracker.AddResourceState(buffer->GetD3DResource(), D3D12_RESOURCE_STATE_COPY_DEST);
			break;
		}
		CORE_INFO("Buffer replaced");
	}
}

void RenderDevice::ReplaceBuffer(const Buffer::Properties& Properties, RenderResourceHandle HeapHandle, UINT64 HeapOffset, RenderResourceHandle ExistingRenderResourceHandle)
{
	const auto pHeap = this->GetHeap(HeapHandle);
	auto [buffer, success] = m_Buffers.ReplaceResource(ExistingRenderResourceHandle, &m_Device, Properties, pHeap, HeapOffset);
	if (success)
	{
		if (pHeap->GetCPUAccessibleHeapType())
		{
			switch (pHeap->GetCPUAccessibleHeapType().value())
			{
			case CPUAccessibleHeapType::Upload:
				m_GlobalResourceStateTracker.AddResourceState(buffer->GetD3DResource(), D3D12_RESOURCE_STATE_GENERIC_READ);
				break;
			case CPUAccessibleHeapType::Readback:
				m_GlobalResourceStateTracker.AddResourceState(buffer->GetD3DResource(), D3D12_RESOURCE_STATE_COPY_DEST);
				break;
			}
		}
		else
		{
			m_GlobalResourceStateTracker.AddResourceState(buffer->GetD3DResource(), Properties.InitialState);
		}
		CORE_INFO("Buffer replaced");
	}
}

void RenderDevice::ReplaceTexture(const Texture::Properties& Properties, RenderResourceHandle ExistingRenderResourceHandle)
{
	auto [texture, success] = m_Textures.ReplaceResource(ExistingRenderResourceHandle, &m_Device, Properties);
	if (success)
	{
		m_GlobalResourceStateTracker.AddResourceState(texture->GetD3DResource(), Properties.InitialState);
		CORE_INFO("Texture replaced");
	}
}

void RenderDevice::ReplaceTexture(const Texture::Properties& Properties, RenderResourceHandle HeapHandle, UINT64 HeapOffset, RenderResourceHandle ExistingRenderResourceHandle)
{
	const auto pHeap = this->GetHeap(HeapHandle);
	auto [texture, success] = m_Textures.ReplaceResource(ExistingRenderResourceHandle, &m_Device, Properties, pHeap, HeapOffset);
	if (success)
	{
		m_GlobalResourceStateTracker.AddResourceState(texture->GetD3DResource(), Properties.InitialState);
		CORE_INFO("Texture replaced");
	}
}

void RenderDevice::ReplaceHeap(const Heap::Properties& Properties, RenderResourceHandle ExistingRenderResourceHandle)
{
	auto [heap, success] = m_Heaps.ReplaceResource(ExistingRenderResourceHandle, &m_Device, Properties);
	if (success)
	{
		CORE_INFO("Heap replaced");
	}
}

void RenderDevice::ReplaceRootSignature(StandardShaderLayoutOptions* pOptions, Delegate<void(RootSignatureProxy&)> Configurator, RenderResourceHandle ExistingRenderResourceHandle)
{
	RootSignatureProxy Proxy;
	Configurator(Proxy);
	if (pOptions)
		AddStandardShaderLayoutRootParameter(pOptions, Proxy);

	auto [rootSignature, success] = m_RootSignatures.ReplaceResource(ExistingRenderResourceHandle, &m_Device, Proxy);
	if (success)
	{
		CORE_INFO("Root Signature replaced");
	}
}

void RenderDevice::ReplaceGraphicsPipelineState(const GraphicsPipelineState::Properties& Properties, RenderResourceHandle ExistingRenderResourceHandle)
{
	auto [graphicsPSO, success] = m_GraphicsPipelineStates.ReplaceResource(ExistingRenderResourceHandle, &m_Device, Properties);
	if (success)
	{
		CORE_INFO("Graphics PSO replaced");
	}
}

void RenderDevice::ReplaceComputePipelineState(const ComputePipelineState::Properties& Properties, RenderResourceHandle ExistingRenderResourceHandle)
{
	auto [computePSO, success] = m_ComputePipelineStates.ReplaceResource(ExistingRenderResourceHandle, &m_Device, Properties);
	if (success)
	{
		CORE_INFO("Compute PSO replaced");
	}
}

void RenderDevice::Destroy(RenderResourceHandle* pRenderResourceHandle)
{
	if (!pRenderResourceHandle)
	{
		CORE_INFO("{} Info: Handle is NULL, Call redudant", __FUNCTION__);
		return;
	}
	if (pRenderResourceHandle->Type == RenderResourceHandle::Types::Unknown)
	{
		CORE_INFO("{} Info: Resource type is unknown, Call redudant", __FUNCTION__);
		return;
	}
	if (pRenderResourceHandle->Flag == RenderResourceHandle::Flags::Destroyed)
	{
		CORE_INFO("{} Info: Resource is already destroyed, Call redudant", __FUNCTION__);
		return;
	}

	switch (pRenderResourceHandle->Type)
	{
	case RenderResourceHandle::Types::Shader: m_Shaders.Destroy(*pRenderResourceHandle, true); break;
	case RenderResourceHandle::Types::Buffer: m_Buffers.Destroy(*pRenderResourceHandle, true); break;
	case RenderResourceHandle::Types::Texture: m_Textures.Destroy(*pRenderResourceHandle, true); break;
	case RenderResourceHandle::Types::Heap: m_Heaps.Destroy(*pRenderResourceHandle, true); break;
	case RenderResourceHandle::Types::RootSignature: m_RootSignatures.Destroy(*pRenderResourceHandle, true); break;
	case RenderResourceHandle::Types::GraphicsPSO: m_GraphicsPipelineStates.Destroy(*pRenderResourceHandle, true); break;
	case RenderResourceHandle::Types::ComputePSO: m_ComputePipelineStates.Destroy(*pRenderResourceHandle, true); break;
	}
	pRenderResourceHandle->Type = RenderResourceHandle::Types::Unknown;
	pRenderResourceHandle->Flag = RenderResourceHandle::Flags::Destroyed;
	pRenderResourceHandle->Data = 0;
}

RenderResourceHandle RenderDevice::CreateSwapChainTexture(Microsoft::WRL::ComPtr<ID3D12Resource> ExistingResource, D3D12_RESOURCE_STATES InitialState)
{
	auto [handle, texture] = m_SwapChainTextures.CreateResource(ExistingResource);
	m_GlobalResourceStateTracker.AddResourceState(ExistingResource.Get(), InitialState);
	return handle;
}

void RenderDevice::DestroySwapChainTexture(RenderResourceHandle* pRenderResourceHandle)
{
	if (!pRenderResourceHandle)
		return;
	m_SwapChainTextures.Destroy(*pRenderResourceHandle, false);
}

void RenderDevice::ReplaceSwapChainTexture(Microsoft::WRL::ComPtr<ID3D12Resource> ExistingResource, D3D12_RESOURCE_STATES InitialState, RenderResourceHandle ExistingRenderResourceHandle)
{
	auto [swapChainTexture, success] = m_SwapChainTextures.ReplaceResource(ExistingRenderResourceHandle, ExistingResource);
	if (success)
	{
		m_GlobalResourceStateTracker.AddResourceState(swapChainTexture->GetD3DResource(), InitialState);
		CORE_INFO("SwapChain resource replaced");
	}
}

void RenderDevice::CreateRTVForSwapChainTexture(RenderResourceHandle RenderResourceHandle, Descriptor DestDescriptor)
{
	Texture* swapChainTexture = m_SwapChainTextures.GetResource(RenderResourceHandle);
	assert(swapChainTexture != nullptr && "Could not find swapchain texture given the handle");

	m_Device.GetD3DDevice()->CreateRenderTargetView(swapChainTexture->GetD3DResource(), nullptr, DestDescriptor.CPUHandle);
}

void RenderDevice::CreateSRV(RenderResourceHandle RenderResourceHandle, Descriptor DestDescriptor)
{
	Texture* texture = GetTexture(RenderResourceHandle);
	assert(texture != nullptr && "Could not find texture given the handle");

	auto getValidSRVFormat = [](DXGI_FORMAT Format)
	{
		if (Format == DXGI_FORMAT_R32_TYPELESS)
			return DXGI_FORMAT_R32_FLOAT;
		return Format;
	};

	D3D12_SHADER_RESOURCE_VIEW_DESC desc = {};
	desc.Format = getValidSRVFormat(texture->Format);
	desc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;

	if (texture->IsCubemap)
	{
		desc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURECUBE;
		desc.TextureCube.MipLevels = texture->MipLevels;
	}
	else
	{
		switch (texture->Type)
		{
		case Resource::Type::Texture1D:
		{
			if (texture->DepthOrArraySize > 1)
			{
				desc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE1DARRAY;
				desc.Texture1DArray.MipLevels = texture->MipLevels;
				desc.Texture1DArray.ArraySize = texture->DepthOrArraySize;
			}
			else
			{
				desc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE1D;
				desc.Texture1D.MipLevels = texture->MipLevels;
			}
		}
		break;

		case Resource::Type::Texture2D:
		{
			if (texture->DepthOrArraySize > 1)
			{
				desc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2DARRAY;
				desc.Texture2DArray.MipLevels = texture->MipLevels;
				desc.Texture2DArray.ArraySize = texture->DepthOrArraySize;
			}
			else
			{
				desc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D;
				desc.Texture2D.MipLevels = texture->MipLevels;
			}
		}
		break;

		case Resource::Type::Texture3D:
		{
			desc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE3D;
			desc.Texture3D.MipLevels = texture->MipLevels;
		}
		break;
		}
	}

	m_Device.GetD3DDevice()->CreateShaderResourceView(texture->GetD3DResource(), &desc, DestDescriptor.CPUHandle);
}

void RenderDevice::CreateUAV(RenderResourceHandle RenderResourceHandle, Descriptor DestDescriptor, std::optional<UINT> ArraySlice, std::optional<UINT> MipSlice)
{
	Texture* texture = GetTexture(RenderResourceHandle);
	assert(texture != nullptr && "Could not find texture given the handle");

	UINT arraySlice = ArraySlice.value_or(0);
	UINT mipSlice = MipSlice.value_or(0);
	D3D12_UNORDERED_ACCESS_VIEW_DESC desc = {};
	desc.Format = texture->Format;

	// TODO: Add buffer support
	switch (texture->Type)
	{
	case Resource::Type::Texture1D:
	{
		if (texture->DepthOrArraySize > 1)
		{
			desc.ViewDimension = D3D12_UAV_DIMENSION_TEXTURE1DARRAY;
			desc.Texture1DArray.MipSlice = mipSlice;
			desc.Texture1DArray.FirstArraySlice = arraySlice;
			desc.Texture1DArray.ArraySize = texture->DepthOrArraySize;
		}
		else
		{
			desc.ViewDimension = D3D12_UAV_DIMENSION_TEXTURE1D;
			desc.Texture1D.MipSlice = mipSlice;
		}
	}
	break;

	case Resource::Type::Texture2D:
	{
		if (texture->DepthOrArraySize > 1)
		{
			desc.ViewDimension = D3D12_UAV_DIMENSION_TEXTURE2DARRAY;
			desc.Texture2DArray.MipSlice = mipSlice;
			desc.Texture2DArray.FirstArraySlice = arraySlice;
			desc.Texture2DArray.ArraySize = texture->DepthOrArraySize;
			desc.Texture2DArray.PlaneSlice = 0;
		}
		else
		{
			desc.ViewDimension = D3D12_UAV_DIMENSION_TEXTURE2D;
			desc.Texture2D.MipSlice = mipSlice;
			desc.Texture2D.PlaneSlice = 0;
		}
	}
	break;

	case Resource::Type::Texture3D:
	{
		desc.ViewDimension = D3D12_UAV_DIMENSION_TEXTURE3D;
		desc.Texture3D.MipSlice = mipSlice;
		desc.Texture3D.FirstWSlice = 0;
		desc.Texture3D.WSize = 0;
	}
	break;
	}

	m_Device.GetD3DDevice()->CreateUnorderedAccessView(texture->GetD3DResource(), nullptr, &desc, DestDescriptor.CPUHandle);
}

void RenderDevice::CreateRTV(RenderResourceHandle RenderResourceHandle, Descriptor DestDescriptor, std::optional<UINT> ArraySlice, std::optional<UINT> MipSlice, std::optional<UINT> ArraySize)
{
	Texture* texture = GetTexture(RenderResourceHandle);
	assert(texture != nullptr && "Could not find texture given the handle");

	UINT arraySlice = ArraySlice.value_or(0);
	UINT mipSlice = MipSlice.value_or(0);
	UINT arraySize = ArraySize.value_or(texture->DepthOrArraySize);
	D3D12_RENDER_TARGET_VIEW_DESC desc = {};
	desc.Format = texture->Format;

	// TODO: Add buffer support and MS support
	switch (texture->Type)
	{
	case Resource::Type::Texture1D:
	{
		if (texture->DepthOrArraySize > 1)
		{
			desc.ViewDimension = D3D12_RTV_DIMENSION_TEXTURE1DARRAY;
			desc.Texture1DArray.MipSlice = mipSlice;
			desc.Texture1DArray.FirstArraySlice = arraySlice;
			desc.Texture1DArray.ArraySize = arraySize;
		}
		else
		{
			desc.ViewDimension = D3D12_RTV_DIMENSION_TEXTURE1D;
			desc.Texture1D.MipSlice = mipSlice;
		}
	}
	break;

	case Resource::Type::Texture2D:
	{
		if (texture->DepthOrArraySize > 1)
		{
			desc.ViewDimension = D3D12_RTV_DIMENSION_TEXTURE2DARRAY;
			desc.Texture2DArray.MipSlice = mipSlice;
			desc.Texture2DArray.FirstArraySlice = arraySlice;
			desc.Texture2DArray.ArraySize = arraySize;
			desc.Texture2DArray.PlaneSlice = 0;
		}
		else
		{
			desc.ViewDimension = D3D12_RTV_DIMENSION_TEXTURE2D;
			desc.Texture2D.MipSlice = mipSlice;
			desc.Texture2D.PlaneSlice = 0;
		}
	}
	break;

	case Resource::Type::Texture3D:
	{
		desc.ViewDimension = D3D12_RTV_DIMENSION_TEXTURE3D;
		desc.Texture3D.MipSlice = mipSlice;
		desc.Texture3D.FirstWSlice = 0;
		desc.Texture3D.WSize = 0;
	}
	break;
	}

	m_Device.GetD3DDevice()->CreateRenderTargetView(texture->GetD3DResource(), &desc, DestDescriptor.CPUHandle);
}

void RenderDevice::CreateDSV(RenderResourceHandle RenderResourceHandle, Descriptor DestDescriptor, std::optional<UINT> ArraySlice, std::optional<UINT> MipSlice, std::optional<UINT> ArraySize)
{
	Texture* texture = GetTexture(RenderResourceHandle);
	assert(texture != nullptr && "Could not find texture given the handle");

	auto getValidDSVFormat = [](DXGI_FORMAT Format)
	{
		// TODO: Add more
		switch (Format)
		{
		case DXGI_FORMAT_R32_TYPELESS: return DXGI_FORMAT_D32_FLOAT;

		default: return DXGI_FORMAT_UNKNOWN;
		}
	};

	UINT arraySlice = ArraySlice.value_or(0);
	UINT mipSlice = MipSlice.value_or(0);
	UINT arraySize = ArraySize.value_or(texture->DepthOrArraySize);
	D3D12_DEPTH_STENCIL_VIEW_DESC desc = {};
	desc.Format = getValidDSVFormat(texture->Format);
	desc.Flags = D3D12_DSV_FLAG_NONE;

	// TODO: Add support and MS support
	switch (texture->Type)
	{
	case Resource::Type::Texture1D:
	{
		if (texture->DepthOrArraySize > 1)
		{
			desc.ViewDimension = D3D12_DSV_DIMENSION_TEXTURE1DARRAY;
			desc.Texture1DArray.MipSlice = mipSlice;
			desc.Texture1DArray.FirstArraySlice = arraySlice;
			desc.Texture1DArray.ArraySize = arraySize;
		}
		else
		{
			desc.ViewDimension = D3D12_DSV_DIMENSION_TEXTURE1D;
			desc.Texture1D.MipSlice = mipSlice;
		}
	}
	break;

	case Resource::Type::Texture2D:
	{
		if (texture->DepthOrArraySize > 1)
		{
			desc.ViewDimension = D3D12_DSV_DIMENSION_TEXTURE2DARRAY;
			desc.Texture2DArray.MipSlice = mipSlice;
			desc.Texture2DArray.FirstArraySlice = arraySlice;
			desc.Texture2DArray.ArraySize = arraySize;
		}
		else
		{
			desc.ViewDimension = D3D12_DSV_DIMENSION_TEXTURE2D;
			desc.Texture2D.MipSlice = mipSlice;
		}
	}
	break;

	case Resource::Type::Texture3D:
	{
		assert("Cannot create DSV for Texture3D resource type");
	}
	break;
	}

	m_Device.GetD3DDevice()->CreateDepthStencilView(texture->GetD3DResource(), &desc, DestDescriptor.CPUHandle);
}

void RenderDevice::AddStandardShaderLayoutRootParameter(StandardShaderLayoutOptions* pOptions, RootSignatureProxy& RootSignatureProxy)
{
	// ConstantDataCB
	if (pOptions->InitConstantDataTypeAsRootConstants)
		RootSignatureProxy.AddRootConstantsParameter(0, 100, pOptions->Num32BitValues);
	else
		RootSignatureProxy.AddRootCBVParameter(0, 100);

	// RenderPassDataCB
	RootSignatureProxy.AddRootCBVParameter(1, 100);

	// Descriptor table for unbound descriptors
	RootSignatureProxy.AddRootDescriptorTableParameter(std::vector<D3D12_DESCRIPTOR_RANGE1>(m_StandardShaderLayoutDescriptorRanges.begin(), m_StandardShaderLayoutDescriptorRanges.end()));

	// Static Samplers
	RootSignatureProxy.AddStaticSampler(0, D3D12_FILTER_MIN_MAG_MIP_POINT, D3D12_TEXTURE_ADDRESS_MODE_WRAP, 16); // PointWrap
	RootSignatureProxy.AddStaticSampler(1, D3D12_FILTER_MIN_MAG_MIP_POINT, D3D12_TEXTURE_ADDRESS_MODE_CLAMP, 16); // PointClamp
	RootSignatureProxy.AddStaticSampler(2, D3D12_FILTER_MIN_MAG_MIP_LINEAR, D3D12_TEXTURE_ADDRESS_MODE_WRAP, 16); // LinearWrap
	RootSignatureProxy.AddStaticSampler(3, D3D12_FILTER_MIN_MAG_MIP_LINEAR, D3D12_TEXTURE_ADDRESS_MODE_CLAMP, 16); // LinearClamp
	RootSignatureProxy.AddStaticSampler(4, D3D12_FILTER_ANISOTROPIC, D3D12_TEXTURE_ADDRESS_MODE_WRAP, 16); // AnisotropicClamp
	RootSignatureProxy.AddStaticSampler(5, D3D12_FILTER_ANISOTROPIC, D3D12_TEXTURE_ADDRESS_MODE_CLAMP, 16); // AnisotropicLinearClamp
	RootSignatureProxy.AddStaticSampler(6, D3D12_FILTER_COMPARISON_MIN_MAG_LINEAR_MIP_POINT, D3D12_TEXTURE_ADDRESS_MODE_BORDER, 16, D3D12_COMPARISON_FUNC_LESS_EQUAL, D3D12_STATIC_BORDER_COLOR_OPAQUE_BLACK); // Shadow
}