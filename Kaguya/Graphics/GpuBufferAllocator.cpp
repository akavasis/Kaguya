#include "pch.h"
#include "GpuBufferAllocator.h"
#include "RendererRegistry.h"

GpuBufferAllocator::GpuBufferAllocator(size_t VertexBufferByteSize, size_t IndexBufferByteSize, size_t ConstantBufferByteSize, RenderDevice* pRenderDevice)
	: m_pRenderDevice(pRenderDevice),
	m_Buffers{ VertexBufferByteSize, IndexBufferByteSize , Math::AlignUp<UINT64>(ConstantBufferByteSize, D3D12_CONSTANT_BUFFER_DATA_PLACEMENT_ALIGNMENT) }
{
	for (size_t i = 0; i < NumInternalBufferTypes; ++i)
	{
		size_t bufferByteSize = m_Buffers[i].Allocator.GetSize();
		RenderResourceHandle bufferHandle, uploadBufferHandle;

		Buffer::Properties bufferProp = {};
		bufferProp.SizeInBytes = bufferByteSize;
		bufferProp.InitialState = D3D12_RESOURCE_STATE_COPY_DEST;
		if (i == ConstantBuffer)
		{
			bufferProp.Stride = Math::AlignUp<UINT>(sizeof(ObjectConstants), D3D12_CONSTANT_BUFFER_DATA_PLACEMENT_ALIGNMENT);
		}

		if (i != ConstantBuffer)
		{
			bufferHandle = m_pRenderDevice->CreateBuffer(bufferProp);
			m_Buffers[i].BufferHandle = bufferHandle;
			m_Buffers[i].pBuffer = m_pRenderDevice->GetBuffer(bufferHandle);
		}

		uploadBufferHandle = m_pRenderDevice->CreateBuffer(bufferProp, CPUAccessibleHeapType::Upload);
		m_Buffers[i].UploadBufferHandle = uploadBufferHandle;
		m_Buffers[i].pUploadBuffer = m_pRenderDevice->GetBuffer(uploadBufferHandle);
		m_Buffers[i].pUploadBuffer->Map();
	}
}

void GpuBufferAllocator::Stage(Scene& Scene, RenderCommandContext* pRenderCommandContext)
{
	// Stage skybox mesh
	{
		auto skyboxModel = CreateBox(1.0f, 1.0f, 1.0f, 0);
		std::vector<Vertex> vertices = std::move(skyboxModel.Vertices);
		std::vector<UINT> indices = std::move(skyboxModel.Indices);

		Scene.Skybox.Mesh.BoundingBox = skyboxModel.BoundingBox;
		Scene.Skybox.Mesh.IndexCount = indices.size();
		Scene.Skybox.Mesh.StartIndexLocation = 0;
		Scene.Skybox.Mesh.VertexCount = vertices.size();
		Scene.Skybox.Mesh.BaseVertexLocation = 0;

		UINT64 totalVertexBytes = vertices.size() * sizeof(Vertex);
		UINT64 totalIndexBytes = indices.size() * sizeof(UINT);

		StageVertex(vertices.data(), totalVertexBytes, pRenderCommandContext);
		StageIndex(indices.data(), totalIndexBytes, pRenderCommandContext);
	}

	for (auto& model : Scene.Models)
	{
		UINT64 totalVertexBytes = model.Vertices.size() * sizeof(Vertex);
		UINT64 totalIndexBytes = model.Indices.size() * sizeof(UINT);

		auto vertexByteOffset = StageVertex(model.Vertices.data(), totalVertexBytes, pRenderCommandContext);
		auto indexByteOffset = StageIndex(model.Indices.data(), totalIndexBytes, pRenderCommandContext);

		for (auto& mesh : model.Meshes)
		{
			assert(vertexByteOffset % sizeof(Vertex) == 0 && "Vertex offset mismatch");
			assert(indexByteOffset % sizeof(UINT) == 0 && "Index offset mismatch");
			mesh.BaseVertexLocation += (vertexByteOffset / sizeof(Vertex));
			mesh.StartIndexLocation += (indexByteOffset / sizeof(UINT));
		}

		CORE_INFO("{} Loaded", model.Path);
	}

	for (auto& model : Scene.Models)
	{
		UINT64 totalConstantBufferBytes = model.Meshes.size() * m_Buffers[ConstantBuffer].pUploadBuffer->GetStride();
		auto pair = m_Buffers[ConstantBuffer].Allocate(totalConstantBufferBytes);
		if (!pair.has_value())
		{
			CORE_WARN("{} : Unable to allocate constant buffer data, consider increasing memory", __FUNCTION__);
			assert(false);
		}
		auto [constantBufferByteOffset, constantBufferByteSize] = pair.value();

		std::size_t constantBufferIndexOffset = constantBufferByteOffset / m_Buffers[ConstantBuffer].pUploadBuffer->GetStride();
		std::size_t constantBufferIndex = 0;
		for (auto& mesh : model.Meshes)
		{
			mesh.ObjectConstantsIndex = constantBufferIndex + constantBufferIndexOffset;
			constantBufferIndex++;
		}
	}

	pRenderCommandContext->TransitionBarrier(m_Buffers[VertexBuffer].pBuffer, D3D12_RESOURCE_STATE_VERTEX_AND_CONSTANT_BUFFER);
	pRenderCommandContext->TransitionBarrier(m_Buffers[IndexBuffer].pBuffer, D3D12_RESOURCE_STATE_INDEX_BUFFER);
	pRenderCommandContext->FlushResourceBarriers();
}

void GpuBufferAllocator::Update(Scene& Scene)
{
	for (auto& model : Scene.Models)
	{
		for (auto& mesh : model.Meshes)
		{
			ObjectConstants ObjectCPU;
			DirectX::XMStoreFloat4x4(&ObjectCPU.World, DirectX::XMMatrixTranspose(mesh.Transform.Matrix()));
			ObjectCPU.MaterialIndex = mesh.MaterialIndex;
			m_Buffers[ConstantBuffer].pUploadBuffer->Update<ObjectConstants>(mesh.ObjectConstantsIndex, ObjectCPU);
		}
	}
}

void GpuBufferAllocator::Bind(RenderCommandContext* pRenderCommandContext) const
{
	D3D12_VERTEX_BUFFER_VIEW vertexBufferView;
	vertexBufferView.BufferLocation = m_Buffers[VertexBuffer].pBuffer->GetGpuVirtualAddress();
	vertexBufferView.SizeInBytes = m_Buffers[VertexBuffer].Allocator.GetCurrentSize();
	vertexBufferView.StrideInBytes = sizeof(Vertex);
	pRenderCommandContext->SetVertexBuffers(0, 1, &vertexBufferView);

	D3D12_INDEX_BUFFER_VIEW indexBufferView;
	indexBufferView.BufferLocation = m_Buffers[IndexBuffer].pBuffer->GetGpuVirtualAddress();
	indexBufferView.SizeInBytes = m_Buffers[IndexBuffer].Allocator.GetCurrentSize();
	indexBufferView.Format = DXGI_FORMAT_R32_UINT;
	pRenderCommandContext->SetIndexBuffer(&indexBufferView);
}

size_t GpuBufferAllocator::StageVertex(const void* pData, size_t ByteSize, RenderCommandContext* pRenderCommandContext)
{
	auto pair = m_Buffers[VertexBuffer].Allocate(ByteSize);
	if (!pair.has_value())
	{
		CORE_WARN("{} : Unable to allocate vertex data, consider increasing memory", __FUNCTION__);
		assert(false);
	}
	auto [vertexOffset, vertexSize] = pair.value();

	// Stage vertex
	auto pGPU = m_Buffers[VertexBuffer].pUploadBuffer->Map();
	auto pCPU = pData;
	memcpy(&pGPU[vertexOffset], pCPU, vertexSize);
	pRenderCommandContext->CopyBufferRegion(m_Buffers[VertexBuffer].pBuffer, vertexOffset, m_Buffers[VertexBuffer].pUploadBuffer, vertexOffset, vertexSize);
	return vertexOffset;
}

size_t GpuBufferAllocator::StageIndex(const void* pData, size_t ByteSize, RenderCommandContext* pRenderCommandContext)
{
	auto pair = m_Buffers[IndexBuffer].Allocate(ByteSize);
	if (!pair.has_value())
	{
		CORE_WARN("{} : Unable to allocate index data, consider increasing memory", __FUNCTION__);
		assert(false);
	}
	auto [indexOffset, indexSize] = pair.value();

	// Stage index
	auto pGPU = m_Buffers[IndexBuffer].pUploadBuffer->Map();
	auto pCPU = pData;
	memcpy(&pGPU[indexOffset], pCPU, indexSize);
	pRenderCommandContext->CopyBufferRegion(m_Buffers[IndexBuffer].pBuffer, indexOffset, m_Buffers[IndexBuffer].pUploadBuffer, indexOffset, indexSize);
	return indexOffset;
}