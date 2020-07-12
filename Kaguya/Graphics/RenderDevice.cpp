#include "pch.h"
#include "RenderDevice.h"

RenderDevice::RenderDevice(IDXGIAdapter4* pAdapter)
	: m_Device(pAdapter),
	m_GraphicsQueue(&m_Device, D3D12_COMMAND_LIST_TYPE_DIRECT),
	m_ComputeQueue(&m_Device, D3D12_COMMAND_LIST_TYPE_COMPUTE),
	m_CopyQueue(&m_Device, D3D12_COMMAND_LIST_TYPE_COPY)
{
}

RenderDevice::~RenderDevice()
{
}

RenderCommandContext* RenderDevice::AllocateContext(D3D12_COMMAND_LIST_TYPE Type)
{
	m_RenderCommandContexts[Type].push_back(std::make_unique<RenderCommandContext>(this, Type));
	return m_RenderCommandContexts[Type].back().get();
}

void RenderDevice::ExecuteRenderCommandContexts(UINT NumRenderCommandContexts, RenderCommandContext* ppRenderCommandContexts[])
{
	std::vector<ID3D12CommandList*> commandlistsToBeExecuted;
	commandlistsToBeExecuted.reserve(NumRenderCommandContexts);
	for (UINT i = 0; i < NumRenderCommandContexts; ++i)
	{
		RenderCommandContext* pRenderCommandContext = ppRenderCommandContexts[i];
		if (pRenderCommandContext->Close(m_GlobalResourceTracker));
		{
			commandlistsToBeExecuted.push_back(pRenderCommandContext->m_pPendingCommandList.Get());
		}
		commandlistsToBeExecuted.push_back(pRenderCommandContext->m_pCommandList.Get());
	}

	m_GraphicsQueue.GetD3DCommandQueue()->ExecuteCommandLists(commandlistsToBeExecuted.size(), commandlistsToBeExecuted.data());
	m_GraphicsQueue.WaitForIdle(); // TODO: REMOVE
}

RenderResourceHandle RenderDevice::LoadShader(Shader::Type Type, LPCWSTR pPath, LPCWSTR pEntryPoint, const std::vector<DxcDefine>& ShaderDefines)
{
	Shader shader = m_ShaderCompiler.LoadShader(Type, pPath, pEntryPoint, ShaderDefines);
	RenderResourceHandle handle = m_Shaders.CreateResource(std::move(shader));
	return handle;
}

RenderResourceHandle RenderDevice::CreateBuffer(const Buffer::Properties& Properties)
{
	RenderResourceHandle handle = m_Buffers.CreateResource(&m_Device, Properties);
	m_GlobalResourceTracker.AddResourceState(m_Buffers.GetResource(handle)->GetD3DResource(), Properties.InitialState);
	return handle;
}

RenderResourceHandle RenderDevice::CreateBuffer(const Buffer::Properties& Properties, CPUAccessibleHeapType CPUAccessibleHeapType, const void* pData)
{
	RenderResourceHandle handle = m_Buffers.CreateResource(&m_Device, Properties, CPUAccessibleHeapType, pData);
	switch (CPUAccessibleHeapType)
	{
	case CPUAccessibleHeapType::Upload:
		m_GlobalResourceTracker.AddResourceState(m_Buffers.GetResource(handle)->GetD3DResource(), D3D12_RESOURCE_STATE_GENERIC_READ);
		break;
	case CPUAccessibleHeapType::Readback:
		m_GlobalResourceTracker.AddResourceState(m_Buffers.GetResource(handle)->GetD3DResource(), D3D12_RESOURCE_STATE_COPY_DEST);
		break;
	}
	return handle;
}

RenderResourceHandle RenderDevice::CreateBuffer(const Buffer::Properties& Properties, RenderResourceHandle HeapHandle, UINT64 HeapOffset)
{
	const auto pHeap = this->GetHeap(HeapHandle);
	RenderResourceHandle handle = m_Buffers.CreateResource(&m_Device, Properties, pHeap, HeapOffset);
	if (pHeap->GetCPUAccessibleHeapType())
	{
		switch (pHeap->GetCPUAccessibleHeapType().value())
		{
		case CPUAccessibleHeapType::Upload:
			m_GlobalResourceTracker.AddResourceState(m_Buffers.GetResource(handle)->GetD3DResource(), D3D12_RESOURCE_STATE_GENERIC_READ);
			break;
		case CPUAccessibleHeapType::Readback:
			m_GlobalResourceTracker.AddResourceState(m_Buffers.GetResource(handle)->GetD3DResource(), D3D12_RESOURCE_STATE_COPY_DEST);
			break;
		}
	}
	else
	{
		m_GlobalResourceTracker.AddResourceState(m_Buffers.GetResource(handle)->GetD3DResource(), Properties.InitialState);
	}
	return handle;
}

RenderResourceHandle RenderDevice::CreateTexture(const Texture::Properties& Properties)
{
	RenderResourceHandle handle = m_Textures.CreateResource(&m_Device, Properties);
	m_GlobalResourceTracker.AddResourceState(m_Textures.GetResource(handle)->GetD3DResource(), Properties.InitialState);
	return handle;
}

RenderResourceHandle RenderDevice::CreateTexture(const Texture::Properties& Properties, RenderResourceHandle HeapHandle, UINT64 HeapOffset)
{
	const auto pHeap = this->GetHeap(HeapHandle);
	RenderResourceHandle handle = m_Textures.CreateResource(&m_Device, Properties, pHeap, HeapOffset);
	m_GlobalResourceTracker.AddResourceState(m_Textures.GetResource(handle)->GetD3DResource(), Properties.InitialState);
	return handle;
}

RenderResourceHandle RenderDevice::CreateHeap(const Heap::Properties& Properties)
{
	return m_Heaps.CreateResource(&m_Device, Properties);
}

RenderResourceHandle RenderDevice::CreateRootSignature(const RootSignature::Properties& Properties)
{
	return m_RootSignatures.CreateResource(&m_Device, Properties);
}

RenderResourceHandle RenderDevice::CreateGraphicsPipelineState(const GraphicsPipelineState::Properties& Properties)
{
	return m_GraphicsPipelineStates.CreateResource(&m_Device, Properties);
}

RenderResourceHandle RenderDevice::CreateComputePipelineState(const ComputePipelineState::Properties& Properties)
{
	return m_ComputePipelineStates.CreateResource(&m_Device, Properties);
}

void RenderDevice::Destroy(RenderResourceHandle& RenderResourceHandle)
{
	if (RenderResourceHandle._Type == RenderResourceHandle::Types::Unknown)
	{
		CORE_INFO("{} Info: Resource type is unknown, Call redudant", __FUNCTION__);
		return;
	}
	if (RenderResourceHandle._Flag == RenderResourceHandle::Flags::Destroyed)
	{
		CORE_INFO("{} Info: Resource is already destroyed, Call redudant", __FUNCTION__);
		return;
	}

	switch (RenderResourceHandle._Type)
	{
	case RenderResourceHandle::Types::Shader: m_Shaders.Destroy(RenderResourceHandle); break;
	case RenderResourceHandle::Types::Buffer: m_Buffers.Destroy(RenderResourceHandle); break;
	case RenderResourceHandle::Types::Texture: m_Textures.Destroy(RenderResourceHandle); break;
	case RenderResourceHandle::Types::Heap: m_Heaps.Destroy(RenderResourceHandle); break;
	case RenderResourceHandle::Types::RootSignature: m_RootSignatures.Destroy(RenderResourceHandle); break;
	case RenderResourceHandle::Types::GraphicsPSO: m_GraphicsPipelineStates.Destroy(RenderResourceHandle); break;
	case RenderResourceHandle::Types::ComputePSO: m_ComputePipelineStates.Destroy(RenderResourceHandle); break;
	}
	RenderResourceHandle._Type = RenderResourceHandle::Types::Unknown;
	RenderResourceHandle._Flag = RenderResourceHandle::Flags::Destroyed;
	RenderResourceHandle._Data = 0;
}

void RenderDevice::CreateSRVForTexture(RenderResourceHandle RenderResourceHandle, D3D12_CPU_DESCRIPTOR_HANDLE DestDescriptor)
{
	Texture* texture = GetTexture(RenderResourceHandle);
	assert(texture != nullptr && "Could not find texture given the handle");

	D3D12_SHADER_RESOURCE_VIEW_DESC desc = {};
	desc.Format = texture->Format;
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
			break;

		case Resource::Type::Texture2D:
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
			break;

		case Resource::Type::Texture3D:
			desc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE3D;
			desc.Texture3D.MipLevels = texture->MipLevels;
			break;
		}
	}

	m_Device.GetD3DDevice()->CreateShaderResourceView(texture->GetD3DResource(), &desc, DestDescriptor);
}

void RenderDevice::CreateUAVForTexture(RenderResourceHandle RenderResourceHandle, D3D12_CPU_DESCRIPTOR_HANDLE DestDescriptor, std::optional<UINT> MipSlice)
{
	Texture* texture = GetTexture(RenderResourceHandle);
	assert(texture != nullptr && "Could not find texture given the handle");

	UINT mipSlice = MipSlice.value_or(0);
	D3D12_UNORDERED_ACCESS_VIEW_DESC desc = {};
	desc.Format = texture->Format;

	switch (texture->Type)
	{
	case Resource::Type::Texture1D:
	{
		if (texture->DepthOrArraySize > 1)
		{
			desc.ViewDimension = D3D12_UAV_DIMENSION_TEXTURE1DARRAY;
			desc.Texture1DArray.MipSlice = mipSlice;
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
			desc.Texture2DArray.ArraySize = texture->DepthOrArraySize;
		}
		else
		{
			desc.ViewDimension = D3D12_UAV_DIMENSION_TEXTURE2D;
			desc.Texture2D.MipSlice = mipSlice;
		}
	}
	break;

	case Resource::Type::Texture3D:
	{
		desc.ViewDimension = D3D12_UAV_DIMENSION_TEXTURE3D;
		desc.Texture3D.MipSlice = mipSlice;
	}
	break;
	}

	m_Device.GetD3DDevice()->CreateUnorderedAccessView(texture->GetD3DResource(), nullptr, &desc, DestDescriptor);
}