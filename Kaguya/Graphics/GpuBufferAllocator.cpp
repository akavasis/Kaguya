#include "pch.h"
#include "GpuBufferAllocator.h"
#include "ModelAllocator.h"

GpuBufferAllocator::GpuBufferAllocator(std::size_t VertexBufferByteSize, std::size_t IndexBufferByteSize, RenderDevice& RefRenderDevice)
	: m_RefRenderDevice(RefRenderDevice),
	m_VertexBufferAllocator(VertexBufferByteSize),
	m_IndexBufferAllocator(IndexBufferByteSize)
{
	Buffer::Properties bufferProp{};
	bufferProp.SizeInBytes = VertexBufferByteSize;
	bufferProp.InitialState = D3D12_RESOURCE_STATE_COPY_DEST;

	m_VertexBuffer = m_RefRenderDevice.CreateBuffer(bufferProp);
	m_UploadVertexBuffer = m_RefRenderDevice.CreateBuffer(bufferProp, CPUAccessibleHeapType::Upload);

	bufferProp.SizeInBytes = IndexBufferByteSize;
	m_IndexBuffer = m_RefRenderDevice.CreateBuffer(bufferProp);
	m_UploadIndexBuffer = m_RefRenderDevice.CreateBuffer(bufferProp, CPUAccessibleHeapType::Upload);

	m_pVertexBuffer = m_RefRenderDevice.GetBuffer(m_VertexBuffer);
	m_pIndexBuffer = m_RefRenderDevice.GetBuffer(m_IndexBuffer);
	m_pUploadVertexBuffer = m_RefRenderDevice.GetBuffer(m_UploadVertexBuffer);
	m_pUploadIndexBuffer = m_RefRenderDevice.GetBuffer(m_UploadIndexBuffer);
}

void GpuBufferAllocator::Stage(RenderCommandContext* pRenderCommandContext)
{
	//for (auto iter = pModelAllocator->m_ModelData.begin(); iter != pModelAllocator->m_ModelData.end(); ++iter)
	//{
	//	auto& modelData = (*iter);

	//	UINT64 totalIndexBytes = modelData.indices.size() * sizeof(UINT);
	//	UINT64 totalVertexBytes = modelData.vertices.size() * sizeof(Vertex);

	//	auto pair = m_VertexBufferAllocator.Allocate(totalVertexBytes);
	//	if (!pair.has_value())
	//	{
	//		CORE_WARN("{} : Unable to allocate vertex data, consider increasing memory", __FUNCTION__);
	//		assert(false);
	//	}
	//	auto [vertexOffset, vertexSize] = pair.value();

	//	pair = m_IndexBufferAllocator.Allocate(totalIndexBytes);
	//	if (!pair.has_value())
	//	{
	//		CORE_WARN("{} : Unable to allocate index data, consider increasing memory", __FUNCTION__);
	//		assert(false);
	//	}
	//	auto [indexOffset, indexSize] = pair.value();

	//	// Stage vertex
	//	{
	//		auto pGPU = m_pUploadVertexBuffer->Map();
	//		auto pCPU = modelData.vertices.data();
	//		memcpy(&pGPU[vertexOffset], pCPU, vertexSize);

	//		pRenderCommandContext->CopyBufferRegion(m_pVertexBuffer, vertexOffset, m_pUploadVertexBuffer, vertexOffset, vertexSize);
	//	}

	//	// Stage index
	//	{
	//		auto pGPU = m_pUploadIndexBuffer->Map();
	//		auto pCPU = modelData.indices.data();
	//		memcpy(&pGPU[indexOffset], pCPU, indexSize);

	//		pRenderCommandContext->CopyBufferRegion(m_pIndexBuffer, indexOffset, m_pUploadIndexBuffer, indexOffset, indexSize);
	//	}

	//	CORE_INFO("{} Loaded", modelData.fileName.generic_string());
	//}
}

void GpuBufferAllocator::Bind(RenderCommandContext* pRenderCommandContext) const
{
	/*D3D12_VERTEX_BUFFER_VIEW vertexBufferView;
	vertexBufferView.BufferLocation = m_pVertexBuffer->GetGPUVirtualAddress();
	vertexBufferView.SizeInBytes = m_VertexBufferAllocator.GetCurrentSize();
	vertexBufferView.StrideInBytes = sizeof(Vertex);
	pRenderCommandContext->SetVertexBuffers(0, 1, &vertexBufferView);

	D3D12_INDEX_BUFFER_VIEW indexBufferView;
	indexBufferView.BufferLocation = m_pIndexBuffer->GetGPUVirtualAddress();
	indexBufferView.SizeInBytes = m_IndexBufferAllocator.GetCurrentSize();
	indexBufferView.Format = DXGI_FORMAT_R32_UINT;
	pRenderCommandContext->SetIndexBuffer(&indexBufferView);*/
}