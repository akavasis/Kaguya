#include "pch.h"
#include "GpuBufferAllocator.h"
#include "RendererRegistry.h"

GpuBufferAllocator::GpuBufferAllocator(std::size_t VertexBufferByteSize, std::size_t IndexBufferByteSize, std::size_t ConstantBufferByteSize, RenderDevice& RefRenderDevice)
	: m_RefRenderDevice(RefRenderDevice),
	m_VertexBufferAllocator(VertexBufferByteSize),
	m_IndexBufferAllocator(IndexBufferByteSize),
	m_ConstantBufferAllocator(ConstantBufferByteSize)
{
	Buffer::Properties bufferProp{};
	bufferProp.SizeInBytes = VertexBufferByteSize;
	bufferProp.InitialState = D3D12_RESOURCE_STATE_COPY_DEST;

	m_VertexBuffer = m_RefRenderDevice.CreateBuffer(bufferProp);
	m_UploadVertexBuffer = m_RefRenderDevice.CreateBuffer(bufferProp, CPUAccessibleHeapType::Upload);

	bufferProp.SizeInBytes = IndexBufferByteSize;
	m_IndexBuffer = m_RefRenderDevice.CreateBuffer(bufferProp);
	m_UploadIndexBuffer = m_RefRenderDevice.CreateBuffer(bufferProp, CPUAccessibleHeapType::Upload);

	bufferProp.SizeInBytes = Math::AlignUp<UINT64>(ConstantBufferByteSize, D3D12_CONSTANT_BUFFER_DATA_PLACEMENT_ALIGNMENT);
	bufferProp.Stride = Math::AlignUp<UINT>(sizeof(ObjectConstants), D3D12_CONSTANT_BUFFER_DATA_PLACEMENT_ALIGNMENT);
	m_ConstantBuffer = m_RefRenderDevice.CreateBuffer(bufferProp, CPUAccessibleHeapType::Upload, nullptr);

	m_pVertexBuffer = m_RefRenderDevice.GetBuffer(m_VertexBuffer);
	m_pIndexBuffer = m_RefRenderDevice.GetBuffer(m_IndexBuffer);
	m_pConstantBuffer = m_RefRenderDevice.GetBuffer(m_ConstantBuffer);
	m_pConstantBuffer->Map();
	m_pUploadVertexBuffer = m_RefRenderDevice.GetBuffer(m_UploadVertexBuffer);
	m_pUploadIndexBuffer = m_RefRenderDevice.GetBuffer(m_UploadIndexBuffer);
}

void GpuBufferAllocator::Stage(Scene& Scene, RenderCommandContext* pRenderCommandContext)
{
	m_VertexBufferAllocator.Reset();
	m_IndexBufferAllocator.Reset();
	m_ConstantBufferAllocator.Reset();

	// Stage skybox mesh
	{
		auto skyboxModel = CreateBox(1.0f, 1.0f, 1.0f, 0, Material());
		std::vector<Vertex> vertices = std::move(skyboxModel.Vertices);
		std::vector<UINT> indices = std::move(skyboxModel.Indices);

		Scene.Skybox.Mesh.BoundingBox = skyboxModel.BoundingBox;
		Scene.Skybox.Mesh.IndexCount = indices.size();
		Scene.Skybox.Mesh.StartIndexLocation = 0;
		Scene.Skybox.Mesh.VertexCount = vertices.size();
		Scene.Skybox.Mesh.BaseVertexLocation = 0;

		UINT64 totalVertexBytes = vertices.size() * sizeof(Vertex);
		UINT64 totalIndexBytes = indices.size() * sizeof(UINT);

		auto pair = m_VertexBufferAllocator.Allocate(totalVertexBytes);
		if (!pair.has_value())
		{
			CORE_WARN("{} : Unable to allocate vertex data, consider increasing memory", __FUNCTION__);
			assert(false);
		}
		auto [vertexOffset, vertexSize] = pair.value();

		pair = m_IndexBufferAllocator.Allocate(totalIndexBytes);
		if (!pair.has_value())
		{
			CORE_WARN("{} : Unable to allocate index data, consider increasing memory", __FUNCTION__);
			assert(false);
		}
		auto [indexOffset, indexSize] = pair.value();

		// Stage vertex
		{
			auto pGPU = m_pUploadVertexBuffer->Map();
			auto pCPU = vertices.data();
			memcpy(&pGPU[vertexOffset], pCPU, vertexSize);

			pRenderCommandContext->CopyBufferRegion(m_pVertexBuffer, vertexOffset, m_pUploadVertexBuffer, vertexOffset, vertexSize);
		}

		// Stage index
		{
			auto pGPU = m_pUploadIndexBuffer->Map();
			auto pCPU = indices.data();
			memcpy(&pGPU[indexOffset], pCPU, indexSize);

			pRenderCommandContext->CopyBufferRegion(m_pIndexBuffer, indexOffset, m_pUploadIndexBuffer, indexOffset, indexSize);
		}
	}

	for (auto iter = Scene.Models.begin(); iter != Scene.Models.end(); ++iter)
	{
		auto& model = (*iter);

		UINT64 totalVertexBytes = model.Vertices.size() * sizeof(Vertex);
		UINT64 totalIndexBytes = model.Indices.size() * sizeof(UINT);

		auto pair = m_VertexBufferAllocator.Allocate(totalVertexBytes);
		if (!pair.has_value())
		{
			CORE_WARN("{} : Unable to allocate vertex data, consider increasing memory", __FUNCTION__);
			assert(false);
		}
		auto [vertexByteOffset, vertexByteSize] = pair.value();

		pair = m_IndexBufferAllocator.Allocate(totalIndexBytes);
		if (!pair.has_value())
		{
			CORE_WARN("{} : Unable to allocate index data, consider increasing memory", __FUNCTION__);
			assert(false);
		}
		auto [indexByteOffset, indexByteSize] = pair.value();

		// Stage vertex
		{
			auto pGPU = m_pUploadVertexBuffer->Map();
			auto pCPU = model.Vertices.data();
			memcpy(&pGPU[vertexByteOffset], pCPU, vertexByteSize);

			pRenderCommandContext->CopyBufferRegion(m_pVertexBuffer, vertexByteOffset, m_pUploadVertexBuffer, vertexByteOffset, vertexByteSize);
		}

		// Stage index
		{
			auto pGPU = m_pUploadIndexBuffer->Map();
			auto pCPU = model.Indices.data();
			memcpy(&pGPU[indexByteOffset], pCPU, indexByteSize);

			pRenderCommandContext->CopyBufferRegion(m_pIndexBuffer, indexByteOffset, m_pUploadIndexBuffer, indexByteOffset, indexByteSize);
		}

		for (auto& mesh : model.Meshes)
		{
			assert(vertexByteOffset % sizeof(Vertex) == 0 && "Vertex offset mismatch");
			assert(indexByteOffset % sizeof(UINT) == 0 && "Index offset mismatch");
			mesh.BaseVertexLocation += (vertexByteOffset / sizeof(Vertex));
			mesh.StartIndexLocation += (indexByteOffset / sizeof(UINT));
		}

		CORE_INFO("{} Loaded", model.Path);
	}

	for (auto iter = Scene.Models.begin(); iter != Scene.Models.end(); ++iter)
	{
		auto& model = (*iter);

		UINT64 totalConstantBufferBytes = model.Meshes.size() * m_pConstantBuffer->GetStride();
		auto pair = m_ConstantBufferAllocator.Allocate(totalConstantBufferBytes);
		if (!pair.has_value())
		{
			CORE_WARN("{} : Unable to allocate constant buffer data, consider increasing memory", __FUNCTION__);
			assert(false);
		}
		auto [constantBufferByteOffset, constantBufferByteSize] = pair.value();

		std::size_t constantBufferIndexOffset = constantBufferByteOffset / m_pConstantBuffer->GetStride();
		std::size_t constantBufferIndex = 0;
		for (auto& mesh : model.Meshes)
		{
			mesh.ConstantBufferIndex = constantBufferIndex + constantBufferIndexOffset;
			constantBufferIndex++;
		}
	}

	pRenderCommandContext->TransitionBarrier(m_pVertexBuffer, D3D12_RESOURCE_STATE_VERTEX_AND_CONSTANT_BUFFER);
	pRenderCommandContext->TransitionBarrier(m_pIndexBuffer, D3D12_RESOURCE_STATE_INDEX_BUFFER);
}

void GpuBufferAllocator::Bind(RenderCommandContext* pRenderCommandContext) const
{
	D3D12_VERTEX_BUFFER_VIEW vertexBufferView;
	vertexBufferView.BufferLocation = m_pVertexBuffer->GetGPUVirtualAddress();
	vertexBufferView.SizeInBytes = m_VertexBufferAllocator.GetCurrentSize();
	vertexBufferView.StrideInBytes = sizeof(Vertex);
	pRenderCommandContext->SetVertexBuffers(0, 1, &vertexBufferView);

	D3D12_INDEX_BUFFER_VIEW indexBufferView;
	indexBufferView.BufferLocation = m_pIndexBuffer->GetGPUVirtualAddress();
	indexBufferView.SizeInBytes = m_IndexBufferAllocator.GetCurrentSize();
	indexBufferView.Format = DXGI_FORMAT_R32_UINT;
	pRenderCommandContext->SetIndexBuffer(&indexBufferView);
}

void GpuBufferAllocator::Update(Scene& Scene)
{
	for (auto iter = Scene.Models.begin(); iter != Scene.Models.end(); ++iter)
	{
		auto& model = (*iter);
		for (auto& mesh : model.Meshes)
		{
			ObjectConstants ObjectCPU;
			DirectX::XMStoreFloat4x4(&ObjectCPU.World, DirectX::XMMatrixTranspose(mesh.Transform.Matrix()));
			m_pConstantBuffer->Update<ObjectConstants>(mesh.ConstantBufferIndex, ObjectCPU);
		}
	}
}