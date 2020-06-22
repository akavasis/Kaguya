template<typename T, bool UseConstantBufferAlignment>
inline RenderResourceHandle RenderDevice::CreateBuffer(const Buffer::Properties<T, UseConstantBufferAlignment>& Properties)
{
	Buffer* buffer = new Buffer(&m_Device, Properties);
	return CreateBuffer(buffer, Properties.InitialState);
}

template<typename T, bool UseConstantBufferAlignment>
inline RenderResourceHandle RenderDevice::CreateBuffer(const Buffer::Properties<T, UseConstantBufferAlignment>& Properties, CPUAccessibleHeapType CPUAccessibleHeapType)
{
	Buffer* buffer = new Buffer(&m_Device, Properties, CPUAccessibleHeapType);
	switch (CPUAccessibleHeapType)
	{
	case CPUAccessibleHeapType::Upload:
		return CreateBuffer(buffer, D3D12_RESOURCE_STATE_GENERIC_READ);
	case CPUAccessibleHeapType::Readback:
		return CreateBuffer(buffer, D3D12_RESOURCE_STATE_COPY_DEST);
	default:
		throw std::exception("Unknown CPUAccessibleHeapType");
		break;
	}
}

template<typename T, bool UseConstantBufferAlignment>
inline RenderResourceHandle RenderDevice::CreateBuffer(const Buffer::Properties<T, UseConstantBufferAlignment>& Properties, RenderResourceHandle HeapHandle, UINT64 HeapOffset)
{
	const auto pHeap = this->GetHeap(HeapHandle);
	Buffer* buffer = new Buffer(&m_Device, Properties, pHeap, HeapOffset);
	if (pHeap->GetCPUAccessibleHeapType())
	{
		switch (pHeap->GetCPUAccessibleHeapType().value())
		{
		case CPUAccessibleHeapType::Upload:
			return CreateBuffer(buffer, D3D12_RESOURCE_STATE_GENERIC_READ);
		case CPUAccessibleHeapType::Readback:
			return CreateBuffer(buffer, D3D12_RESOURCE_STATE_COPY_DEST);
		}
	}
	else
	{
		return CreateBuffer(buffer, Properties.InitialState);
	}
}