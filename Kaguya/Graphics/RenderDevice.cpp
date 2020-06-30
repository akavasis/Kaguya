#include "pch.h"
#include "RenderDevice.h"

RenderDevice::RenderDevice(IUnknown* pAdapter)
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
	switch (Type)
	{
	case D3D12_COMMAND_LIST_TYPE::D3D12_COMMAND_LIST_TYPE_DIRECT:
		m_RenderCommandContexts[Type].push_back(std::make_unique<GraphicsCommandContext>(this));
		return m_RenderCommandContexts[Type].back().get();
	case D3D12_COMMAND_LIST_TYPE::D3D12_COMMAND_LIST_TYPE_COMPUTE:
		m_RenderCommandContexts[Type].push_back(std::make_unique<ComputeCommandContext>(this));
		return m_RenderCommandContexts[Type].back().get();
	case D3D12_COMMAND_LIST_TYPE::D3D12_COMMAND_LIST_TYPE_COPY:
		m_RenderCommandContexts[Type].push_back(std::make_unique<CopyCommandContext>(this));
		return m_RenderCommandContexts[Type].back().get();
	}
	return nullptr;
}

RenderResourceHandle RenderDevice::LoadShader(Shader::Type Type, LPCWSTR pPath, LPCWSTR pEntryPoint, const std::vector<DxcDefine>& ShaderDefines)
{
	Shader shader = m_ShaderCompiler.LoadShader(Type, pPath, pEntryPoint, ShaderDefines);
	RenderResourceHandle handle = m_Shaders.CreateResource(std::move(shader));
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
	if (RenderResourceHandle._Type == RenderResourceHandle::Type::Unknown)
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
	case RenderResourceHandle::Type::Buffer: m_Buffers.Destroy(RenderResourceHandle); break;
	case RenderResourceHandle::Type::Texture: m_Textures.Destroy(RenderResourceHandle); break;
	case RenderResourceHandle::Type::RootSignature: m_RootSignatures.Destroy(RenderResourceHandle); break;
	case RenderResourceHandle::Type::GraphicsPSO: m_GraphicsPipelineStates.Destroy(RenderResourceHandle); break;
	case RenderResourceHandle::Type::ComputePSO: m_ComputePipelineStates.Destroy(RenderResourceHandle); break;
	}
	RenderResourceHandle._Type = RenderResourceHandle::Type::Unknown;
	RenderResourceHandle._Flag = RenderResourceHandle::Flags::Destroyed;
	RenderResourceHandle._Data = 0;
}

Shader* RenderDevice::GetShader(RenderResourceHandle RenderResourceHandle) const
{
	return m_Shaders.GetResource(RenderResourceHandle);
}

Buffer* RenderDevice::GetBuffer(RenderResourceHandle RenderResourceHandle) const
{
	return m_Buffers.GetResource(RenderResourceHandle);
}

Texture* RenderDevice::GetTexture(RenderResourceHandle RenderResourceHandle) const
{
	return m_Textures.GetResource(RenderResourceHandle);
}

Heap* RenderDevice::GetHeap(RenderResourceHandle RenderResourceHandle) const
{
	return m_Heaps.GetResource(RenderResourceHandle);
}

RootSignature* RenderDevice::GetRootSignature(RenderResourceHandle RenderResourceHandle) const
{
	return m_RootSignatures.GetResource(RenderResourceHandle);
}

GraphicsPipelineState* RenderDevice::GetGraphicsPSO(RenderResourceHandle RenderResourceHandle) const
{
	return m_GraphicsPipelineStates.GetResource(RenderResourceHandle);
}

ComputePipelineState* RenderDevice::GetComputePSO(RenderResourceHandle RenderResourceHandle) const
{
	return m_ComputePipelineStates.GetResource(RenderResourceHandle);
}