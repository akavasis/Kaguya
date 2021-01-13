#include "pch.h"
#include "BufferManager.h"

BufferManager::BufferManager(RenderDevice* pRenderDevice)
	: pRenderDevice(pRenderDevice)
{

}

void BufferManager::Stage(Scene& Scene, CommandContext* pCommandContext)
{
	for (auto& Mesh : Scene.Meshes)
	{
		LoadMesh(Mesh);
	}

	for (auto& [handle, stagingBuffer] : m_UnstagedBuffers)
	{
		StageBuffer(handle, stagingBuffer, pCommandContext);
	}
	pCommandContext->FlushResourceBarriers();
}

void BufferManager::DisposeResources()
{
	m_UnstagedBuffers.clear();
}

BufferManager::StagingBuffer BufferManager::CreateStagingBuffer(D3D12_RESOURCE_DESC Desc, const void* pData)
{
	auto HeapProperties = CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_UPLOAD);
	Microsoft::WRL::ComPtr<ID3D12Resource> StagingResource;
	pRenderDevice->Device.GetApiHandle()->CreateCommittedResource(&HeapProperties, D3D12_HEAP_FLAG_NONE,
		&Desc, D3D12_RESOURCE_STATE_GENERIC_READ, nullptr, IID_PPV_ARGS(StagingResource.ReleaseAndGetAddressOf()));

	BYTE* pMemory = nullptr;
	if (StagingResource->Map(0, nullptr, reinterpret_cast<void**>(&pMemory)) == S_OK)
	{
		memcpy(pMemory, pData, Desc.Width);

		StagingResource->Unmap(0, nullptr);
	}

	StagingBuffer StagingBuffer = {};
	StagingBuffer.Buffer = Buffer(StagingResource);
	return StagingBuffer;
}

void BufferManager::LoadMesh(Mesh& Mesh)
{
	StageVertexResource(Mesh);
	StageIndexResource(Mesh);

	Mesh.MeshletResource = pRenderDevice->InitializeRenderResourceHandle(RenderResourceType::Heap, Mesh.Name + " [Meshlet Resource]");
	Mesh.VertexIndexResource = pRenderDevice->InitializeRenderResourceHandle(RenderResourceType::Heap, Mesh.Name + " [Vertex Index Resource]");
	Mesh.PrimitiveIndexResource = pRenderDevice->InitializeRenderResourceHandle(RenderResourceType::Heap, Mesh.Name + " [Primitive Index Resource]");

	UINT64 MeshletResourceSizeInBytes = Mesh.Meshlets.size() * sizeof(Meshlet);
	UINT64 VertexIndexResourceSizeInBytes = Mesh.VertexIndices.size() * sizeof(uint32_t);
	UINT64 PrimitiveIndexResourceSizeInBytes = Mesh.PrimitiveIndices.size() * sizeof(MeshletPrimitive);

	D3D12_RESOURCE_DESC MeshletDesc = CD3DX12_RESOURCE_DESC::Buffer(MeshletResourceSizeInBytes);
	D3D12_RESOURCE_DESC VertexIndexDesc = CD3DX12_RESOURCE_DESC::Buffer(VertexIndexResourceSizeInBytes);
	D3D12_RESOURCE_DESC PrimitiveIndexDesc = CD3DX12_RESOURCE_DESC::Buffer(PrimitiveIndexResourceSizeInBytes);

	// Create MeshletResource
	{
		pRenderDevice->CreateBuffer(Mesh.MeshletResource, [&](BufferProxy& proxy)
		{
			proxy.SetSizeInBytes(MeshletResourceSizeInBytes);
			proxy.SetStride(sizeof(Meshlet));
			proxy.InitialState = Resource::State::CopyDest;
		});

		m_UnstagedBuffers[Mesh.MeshletResource] = CreateStagingBuffer(MeshletDesc, Mesh.Meshlets.data());
	}

	// Create VertexIndexHeap and VertexIndexResource
	{
		pRenderDevice->CreateBuffer(Mesh.VertexIndexResource, [&](BufferProxy& proxy)
		{
			proxy.SetSizeInBytes(VertexIndexResourceSizeInBytes);
			proxy.SetStride(sizeof(uint32_t));
			proxy.InitialState = Resource::State::CopyDest;
		});

		m_UnstagedBuffers[Mesh.VertexIndexResource] = CreateStagingBuffer(VertexIndexDesc, Mesh.VertexIndices.data());
	}

	// Create PrimitiveIndexHeap and PrimitiveIndexResource
	{
		pRenderDevice->CreateBuffer(Mesh.PrimitiveIndexResource, [&](BufferProxy& proxy)
		{
			proxy.SetSizeInBytes(PrimitiveIndexResourceSizeInBytes);
			proxy.SetStride(sizeof(MeshletPrimitive));
			proxy.InitialState = Resource::State::CopyDest;
		});

		m_UnstagedBuffers[Mesh.PrimitiveIndexResource] = CreateStagingBuffer(PrimitiveIndexDesc, Mesh.PrimitiveIndices.data());
	}
}

void BufferManager::StageVertexResource(Mesh& Mesh)
{
	Mesh.VertexResource = pRenderDevice->InitializeRenderResourceHandle(RenderResourceType::Buffer, Mesh.Name + "[Vertex Resource]");
	UINT64 VertexResourceSizeInBytes = Mesh.Vertices.size() * sizeof(Vertex);

	pRenderDevice->CreateBuffer(Mesh.VertexResource, [=](BufferProxy& proxy)
	{
		proxy.SetSizeInBytes(VertexResourceSizeInBytes);
		proxy.SetStride(sizeof(Vertex));
		proxy.InitialState = Resource::State::CopyDest;
	});

	Buffer* pBuffer = pRenderDevice->GetBuffer(Mesh.VertexResource);
	m_UnstagedBuffers[Mesh.VertexResource] = CreateStagingBuffer(pBuffer->GetApiHandle()->GetDesc(), Mesh.Vertices.data());
}

void BufferManager::StageIndexResource(Mesh& Mesh)
{
	Mesh.IndexResource = pRenderDevice->InitializeRenderResourceHandle(RenderResourceType::Buffer, Mesh.Name + "[Index Resource]");
	UINT64 IndexResourceSizeInBytes = Mesh.Indices.size() * sizeof(uint32_t);

	pRenderDevice->CreateBuffer(Mesh.IndexResource, [=](BufferProxy& proxy)
	{
		proxy.SetSizeInBytes(IndexResourceSizeInBytes);
		proxy.SetStride(sizeof(uint32_t));
		proxy.InitialState = Resource::State::CopyDest;
	});

	Buffer* pBuffer = pRenderDevice->GetBuffer(Mesh.IndexResource);
	m_UnstagedBuffers[Mesh.IndexResource] = CreateStagingBuffer(pBuffer->GetApiHandle()->GetDesc(), Mesh.Indices.data());
}

void BufferManager::StageBuffer(RenderResourceHandle Handle, StagingBuffer& StagingBuffer, CommandContext* pCommandContext)
{
	Buffer* pBuffer = pRenderDevice->GetBuffer(Handle);
	Buffer* pUploadBuffer = &StagingBuffer.Buffer;

	pCommandContext->CopyBufferRegion(pBuffer, 0, pUploadBuffer, 0, pUploadBuffer->GetMemoryRequested());
	pCommandContext->TransitionBarrier(pBuffer, Resource::State::PixelShaderResource | Resource::State::NonPixelShaderResource);
}