#include "pch.h"
#include "RenderDevice.h"

RenderDevice::RenderDevice(IDXGIAdapter4* pAdapter)
	: m_Device(pAdapter),
	m_GraphicsQueue(GetDevice(), D3D12_COMMAND_LIST_TYPE_DIRECT),
	m_ComputeQueue(GetDevice(), D3D12_COMMAND_LIST_TYPE_COMPUTE),
	m_CopyQueue(GetDevice(), D3D12_COMMAND_LIST_TYPE_COPY),
	m_DescriptorAllocator(GetDevice())
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

CommandContext* RenderDevice::AllocateContext(CommandContext::Type Type)
{
	CommandQueue* pCommandQueue = nullptr;
	switch (Type)
	{
	case CommandContext::Type::Direct: pCommandQueue = &m_GraphicsQueue; break;
	case CommandContext::Type::Compute: pCommandQueue = &m_ComputeQueue; break;
	case CommandContext::Type::Copy: pCommandQueue = &m_CopyQueue; break;
	default: throw std::logic_error("Unsupported command list type"); break;
	}
	m_RenderCommandContexts[Type].push_back(std::make_unique<CommandContext>(&m_Device, pCommandQueue, Type));
	return m_RenderCommandContexts[Type].back().get();
}

void RenderDevice::BindUniversalGpuDescriptorHeap(CommandContext* pCommandContext) const
{
	pCommandContext->SetDescriptorHeaps(m_DescriptorAllocator.GetUniversalGpuDescriptorHeap(), nullptr);
}

void RenderDevice::ExecuteRenderCommandContexts(UINT NumCommandContexts, CommandContext* ppCommandContexts[])
{
	std::vector<ID3D12CommandList*> commandlistsToBeExecuted;
	commandlistsToBeExecuted.reserve(size_t(NumCommandContexts) * 2);
	for (UINT i = 0; i < NumCommandContexts; ++i)
	{
		CommandContext* pCommandContext = ppCommandContexts[i];
		if (pCommandContext->Close(m_GlobalResourceStateTracker))
		{
			commandlistsToBeExecuted.push_back(pCommandContext->m_pPendingCommandList.Get());
		}
		commandlistsToBeExecuted.push_back(pCommandContext->m_pCommandList.Get());
	}

	m_GraphicsQueue.GetD3DCommandQueue()->ExecuteCommandLists(commandlistsToBeExecuted.size(), commandlistsToBeExecuted.data());
	UINT64 fenceValue = m_GraphicsQueue.Signal();
	for (UINT i = 0; i < NumCommandContexts; ++i)
	{
		CommandContext* pCommandContext = ppCommandContexts[i];
		pCommandContext->RequestNewAllocator(fenceValue);
		pCommandContext->Reset();
	}
	m_GraphicsQueue.Flush();
}

RenderResourceHandle RenderDevice::CompileShader(Shader::Type Type, LPCWSTR pPath, LPCWSTR pEntryPoint, const std::vector<DxcDefine>& ShaderDefines)
{
	auto [handle, shader] = m_Shaders.CreateResource(m_ShaderCompiler.CompileShader(Type, pPath, pEntryPoint, ShaderDefines));
	return handle;
}

RenderResourceHandle RenderDevice::CompileLibrary(LPCWSTR pPath)
{
	auto [handle, library] = m_Libraries.CreateResource(m_ShaderCompiler.CompileLibrary(pPath));
	return handle;
}

RenderResourceHandle RenderDevice::CreateBuffer(Delegate<void(BufferProxy&)> Configurator)
{
	BufferProxy proxy;
	Configurator(proxy);

	auto [handle, buffer] = m_Buffers.CreateResource(&m_Device, proxy);

	// No need to track resources that have preinitialized states
	if (buffer->GetCpuAccess() == Buffer::CpuAccess::None)
		m_GlobalResourceStateTracker.AddResourceState(buffer->GetD3DResource(), GetD3DResourceStates(proxy.InitialState));
	return handle;
}

RenderResourceHandle RenderDevice::CreateBuffer(RenderResourceHandle HeapHandle, UINT64 HeapOffset, Delegate<void(BufferProxy&)> Configurator)
{
	BufferProxy proxy;
	Configurator(proxy);

	const auto pHeap = this->GetHeap(HeapHandle);
	auto [handle, buffer] = m_Buffers.CreateResource(&m_Device, pHeap, HeapOffset, proxy);
	m_GlobalResourceStateTracker.AddResourceState(buffer->GetD3DResource(), GetD3DResourceStates(proxy.InitialState));
	return handle;
}

RenderResourceHandle RenderDevice::CreateTexture(Microsoft::WRL::ComPtr<ID3D12Resource> ExistingResource, Resource::State InitialState)
{
	auto [handle, texture] = m_Textures.CreateResource(ExistingResource);
	m_GlobalResourceStateTracker.AddResourceState(ExistingResource.Get(), GetD3DResourceStates(InitialState));
	return handle;
}

RenderResourceHandle RenderDevice::CreateTexture(Resource::Type Type, Delegate<void(TextureProxy&)> Configurator)
{
	TextureProxy proxy(Type);
	Configurator(proxy);

	auto [handle, texture] = m_Textures.CreateResource(&m_Device, proxy);
	m_GlobalResourceStateTracker.AddResourceState(texture->GetD3DResource(), GetD3DResourceStates(proxy.InitialState));
	return handle;
}

RenderResourceHandle RenderDevice::CreateTexture(Resource::Type Type, RenderResourceHandle HeapHandle, UINT64 HeapOffset, Delegate<void(TextureProxy&)> Configurator)
{
	TextureProxy proxy(Type);
	Configurator(proxy);

	const auto pHeap = this->GetHeap(HeapHandle);
	assert(pHeap->GetType() != Heap::Type::Upload && "Heap cannot be type upload");
	assert(pHeap->GetType() != Heap::Type::Readback && "Heap cannot be type readback");

	auto [handle, texture] = m_Textures.CreateResource(&m_Device, pHeap, HeapOffset, proxy);
	m_GlobalResourceStateTracker.AddResourceState(texture->GetD3DResource(), GetD3DResourceStates(proxy.InitialState));
	return handle;
}

RenderResourceHandle RenderDevice::CreateHeap(Delegate<void(HeapProxy&)> Configurator)
{
	HeapProxy proxy;
	Configurator(proxy);

	auto [handle, heap] = m_Heaps.CreateResource(&m_Device, proxy);
	return handle;
}

RenderResourceHandle RenderDevice::CreateRootSignature(StandardShaderLayoutOptions* pOptions, Delegate<void(RootSignatureProxy&)> Configurator)
{
	RootSignatureProxy proxy;
	Configurator(proxy);
	if (pOptions)
		AppendStandardShaderLayoutRootParameter(pOptions, proxy);

	auto [handle, rootSignature] = m_RootSignatures.CreateResource(&m_Device, proxy);
	return handle;
}

RenderResourceHandle RenderDevice::CreateGraphicsPipelineState(Delegate<void(GraphicsPipelineStateProxy&)> Configurator)
{
	GraphicsPipelineStateProxy proxy;
	Configurator(proxy);

	auto [handle, graphicsPSO] = m_GraphicsPipelineStates.CreateResource(&m_Device, proxy);
	return handle;
}

RenderResourceHandle RenderDevice::CreateComputePipelineState(Delegate<void(ComputePipelineStateProxy&)> Configurator)
{
	ComputePipelineStateProxy proxy;
	Configurator(proxy);

	auto [handle, computePSO] = m_ComputePipelineStates.CreateResource(&m_Device, proxy);
	return handle;
}

RenderResourceHandle RenderDevice::CreateRaytracingPipelineState(Delegate<void(RaytracingPipelineStateProxy&)> Configurator)
{
	RaytracingPipelineStateProxy proxy;
	Configurator(proxy);

	auto [handle, raytracingPSO] = m_RaytracingPipelineStates.CreateResource(&m_Device, proxy);
	return handle;
}

void RenderDevice::Destroy(RenderResourceHandle* pRenderResourceHandle)
{
	if (!pRenderResourceHandle)
	{
		CORE_INFO("{} Info: Handle is NULL, Call redudant", __FUNCTION__);
		return;
	}
	if (pRenderResourceHandle->Type == RenderResourceType::Unknown)
	{
		CORE_INFO("{} Info: Resource type is unknown, Call redudant", __FUNCTION__);
		return;
	}
	if (pRenderResourceHandle->Flags == RenderResourceFlags::Destroyed)
	{
		CORE_INFO("{} Info: Resource is already destroyed, Call redudant", __FUNCTION__);
		return;
	}

	switch (pRenderResourceHandle->Type)
	{
	case RenderResourceType::Shader: m_Shaders.Destroy(pRenderResourceHandle); break;
	case RenderResourceType::Buffer:
	{
		auto buffer = m_Buffers.GetResource(*pRenderResourceHandle);
		if (buffer)
		{
			m_GlobalResourceStateTracker.RemoveResourceState(buffer->GetD3DResource());
		}
		m_Buffers.Destroy(pRenderResourceHandle);
	}
	break;
	case RenderResourceType::Texture:
	{
		auto texture = m_Textures.GetResource(*pRenderResourceHandle);
		if (texture)
		{
			m_GlobalResourceStateTracker.RemoveResourceState(texture->GetD3DResource());
		}
		m_Textures.Destroy(pRenderResourceHandle);
	}
	break;
	case RenderResourceType::Heap: m_Heaps.Destroy(pRenderResourceHandle); break;
	case RenderResourceType::RootSignature: m_RootSignatures.Destroy(pRenderResourceHandle); break;
	case RenderResourceType::GraphicsPSO: m_GraphicsPipelineStates.Destroy(pRenderResourceHandle); break;
	case RenderResourceType::ComputePSO: m_ComputePipelineStates.Destroy(pRenderResourceHandle); break;
	}
	pRenderResourceHandle->Type = RenderResourceType::Unknown;
	pRenderResourceHandle->Flags = RenderResourceFlags::Destroyed;
	pRenderResourceHandle->Data = 0;
}

void RenderDevice::CreateSRV(RenderResourceHandle RenderResourceHandle, Descriptor DestDescriptor)
{
	D3D12_SHADER_RESOURCE_VIEW_DESC desc = {};
	switch (RenderResourceHandle.Type)
	{
	case RenderResourceType::Buffer:
	{
		Buffer* pBuffer = GetBuffer(RenderResourceHandle);

		if (EnumMaskBitSet(pBuffer->GetBindFlags(), Resource::BindFlags::AccelerationStructure))
		{
			desc.Format = DXGI_FORMAT_UNKNOWN;
			desc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
			desc.ViewDimension = D3D12_SRV_DIMENSION_RAYTRACING_ACCELERATION_STRUCTURE;
			desc.RaytracingAccelerationStructure.Location = pBuffer->GetGpuVirtualAddress();

			m_Device.GetD3DDevice()->CreateShaderResourceView(nullptr, &desc, DestDescriptor.CPUHandle);
		}
		else
		{
			desc.Format = DXGI_FORMAT_UNKNOWN;
			desc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
			desc.ViewDimension = D3D12_SRV_DIMENSION_BUFFER;
			desc.Buffer.FirstElement = 0;
			desc.Buffer.NumElements = pBuffer->GetMemoryRequested() / pBuffer->GetStride();
			desc.Buffer.StructureByteStride = pBuffer->GetStride();
			desc.Buffer.Flags = D3D12_BUFFER_SRV_FLAG_NONE;

			m_Device.GetD3DDevice()->CreateShaderResourceView(pBuffer->GetD3DResource(), &desc, DestDescriptor.CPUHandle);
		}
	}
	break;

	case RenderResourceType::Texture:
	{
		Texture* pTexture = GetTexture(RenderResourceHandle);

		auto getValidSRVFormat = [](DXGI_FORMAT Format)
		{
			if (Format == DXGI_FORMAT_R32_TYPELESS)
				return DXGI_FORMAT_R32_FLOAT;
			return Format;
		};

		desc.Format = getValidSRVFormat(pTexture->GetFormat());
		desc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;

		switch (pTexture->GetType())
		{
		case Resource::Type::Texture1D:
		{
			if (pTexture->GetDepthOrArraySize() > 1)
			{
				desc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE1DARRAY;
				desc.Texture1DArray.MipLevels = pTexture->GetMipLevels();
				desc.Texture1DArray.ArraySize = pTexture->GetDepthOrArraySize();
			}
			else
			{
				desc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE1D;
				desc.Texture1D.MipLevels = pTexture->GetMipLevels();
			}
		}
		break;

		case Resource::Type::Texture2D:
		{
			if (pTexture->GetDepthOrArraySize() > 1)
			{
				desc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2DARRAY;
				desc.Texture2DArray.MipLevels = pTexture->GetMipLevels();
				desc.Texture2DArray.ArraySize = pTexture->GetDepthOrArraySize();
			}
			else
			{
				desc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D;
				desc.Texture2D.MipLevels = pTexture->GetMipLevels();
			}
		}
		break;

		case Resource::Type::Texture3D:
		{
			desc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE3D;
			desc.Texture3D.MipLevels = pTexture->GetMipLevels();
		}
		break;

		case Resource::Type::TextureCube:
		{
			desc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURECUBE;
			desc.TextureCube.MipLevels = pTexture->GetMipLevels();
		}
		break;
		}

		m_Device.GetD3DDevice()->CreateShaderResourceView(pTexture->GetD3DResource(), &desc, DestDescriptor.CPUHandle);
	}
	break;

	default:
		throw std::logic_error("Could not find resource given the handle");
	}
}

void RenderDevice::CreateUAV(RenderResourceHandle RenderResourceHandle, Descriptor DestDescriptor, std::optional<UINT> ArraySlice, std::optional<UINT> MipSlice)
{
	Texture* texture = GetTexture(RenderResourceHandle);
	if (!texture)
	{
		throw std::logic_error("Could not find texture given the handle");
		return;
	}

	UINT arraySlice = ArraySlice.value_or(0);
	UINT mipSlice = MipSlice.value_or(0);
	D3D12_UNORDERED_ACCESS_VIEW_DESC desc = {};
	desc.Format = texture->GetFormat();

	// TODO: Add buffer support
	switch (texture->GetType())
	{
	case Resource::Type::Texture1D:
	{
		if (texture->GetDepthOrArraySize() > 1)
		{
			desc.ViewDimension = D3D12_UAV_DIMENSION_TEXTURE1DARRAY;
			desc.Texture1DArray.MipSlice = mipSlice;
			desc.Texture1DArray.FirstArraySlice = arraySlice;
			desc.Texture1DArray.ArraySize = texture->GetDepthOrArraySize();
		}
		else
		{
			desc.ViewDimension = D3D12_UAV_DIMENSION_TEXTURE1D;
			desc.Texture1D.MipSlice = mipSlice;
		}
	}
	break;

	case Resource::Type::Texture2D: [[fallthrough]];
	case Resource::Type::TextureCube:
	{
		if (texture->GetDepthOrArraySize() > 1)
		{
			desc.ViewDimension = D3D12_UAV_DIMENSION_TEXTURE2DARRAY;
			desc.Texture2DArray.MipSlice = mipSlice;
			desc.Texture2DArray.FirstArraySlice = arraySlice;
			desc.Texture2DArray.ArraySize = texture->GetDepthOrArraySize();
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
	if (!texture)
	{
		throw std::logic_error("Could not find texture given the handle");
		return;
	}

	UINT arraySlice = ArraySlice.value_or(0);
	UINT mipSlice = MipSlice.value_or(0);
	UINT arraySize = ArraySize.value_or(texture->GetDepthOrArraySize());
	D3D12_RENDER_TARGET_VIEW_DESC desc = {};
	desc.Format = texture->GetFormat();

	// TODO: Add buffer support and MS support
	switch (texture->GetType())
	{
	case Resource::Type::Texture1D:
	{
		if (texture->GetDepthOrArraySize() > 1)
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

	case Resource::Type::Texture2D: [[fallthrough]];
	case Resource::Type::TextureCube:
	{
		if (texture->GetDepthOrArraySize() > 1)
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
	if (!texture)
	{
		throw std::logic_error("Could not find texture given the handle");
		return;
	}

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
	UINT arraySize = ArraySize.value_or(texture->GetDepthOrArraySize());
	D3D12_DEPTH_STENCIL_VIEW_DESC desc = {};
	desc.Format = getValidDSVFormat(texture->GetFormat());
	desc.Flags = D3D12_DSV_FLAG_NONE;

	// TODO: Add support and MS support
	switch (texture->GetType())
	{
	case Resource::Type::Texture1D:
	{
		if (texture->GetDepthOrArraySize() > 1)
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

	case Resource::Type::Texture2D: [[fallthrough]];
	case Resource::Type::TextureCube:
	{
		if (texture->GetDepthOrArraySize() > 1)
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
		throw std::logic_error("Cannot create DSV for Texture3D resource type");
	}
	break;
	}

	m_Device.GetD3DDevice()->CreateDepthStencilView(texture->GetD3DResource(), &desc, DestDescriptor.CPUHandle);
}

void RenderDevice::AppendStandardShaderLayoutRootParameter(StandardShaderLayoutOptions* pOptions, RootSignatureProxy& RootSignatureProxy)
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