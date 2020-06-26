template<typename T, bool UseConstantBufferAlignment>
inline RenderResourceHandle RenderDevice::CreateBuffer(const Buffer::Properties<T, UseConstantBufferAlignment>& Properties)
{
	RenderResourceHandle handle = m_Buffers.CreateResource(&m_Device, Properties);
	m_GlobalResourceTracker.AddResourceState(m_Buffers.GetResource(handle)->GetD3DResource(), Properties.InitialState);
	return handle;
}

template<typename T, bool UseConstantBufferAlignment>
inline RenderResourceHandle RenderDevice::CreateBuffer(const Buffer::Properties<T, UseConstantBufferAlignment>& Properties, CPUAccessibleHeapType CPUAccessibleHeapType, const void* pData)
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
	default:
		assert("Unknown CPUAccessibleHeapType" && false);
		break;
	}
	return handle;
}

template<typename T, bool UseConstantBufferAlignment>
inline RenderResourceHandle RenderDevice::CreateBuffer(const Buffer::Properties<T, UseConstantBufferAlignment>& Properties, RenderResourceHandle HeapHandle, UINT64 HeapOffset)
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