#include "pch.h"
#include "BufferManager.h"

using Microsoft::WRL::ComPtr;

BufferManager::BufferManager(RenderDevice* pRenderDevice)
	: pRenderDevice(pRenderDevice),
	pAllocator(pRenderDevice->Device.Allocator())
{

}

void BufferManager::Stage(Scene& Scene, CommandList& CommandList)
{
	for (auto& Mesh : Scene.Meshes)
	{
		LoadMesh(Mesh);
	}

	for (auto& [handle, stagingBuffer] : m_UnstagedBuffers)
	{
		auto pBuffer = pRenderDevice->GetBuffer(handle)->GetApiHandle();
		auto pUploadBuffer = stagingBuffer.Buffer.Get();

		UINT64 NumBytes = pUploadBuffer->GetDesc().Width;

		CommandList.TransitionBarrier(pBuffer, Resource::State::CopyDest);
		CommandList->CopyBufferRegion(pBuffer, 0, pUploadBuffer, 0, NumBytes);
		CommandList.TransitionBarrier(pBuffer, Resource::State::PixelShaderResource | Resource::State::NonPixelShaderResource);
	}
	CommandList.FlushResourceBarriers();
}

void BufferManager::DisposeResources()
{
	for (auto& [handle, stagingBuffer] : m_UnstagedBuffers)
	{
		stagingBuffer.pAllocation->Release();
	}
	m_UnstagedBuffers.clear();
}

BufferManager::StagingBuffer BufferManager::CreateStagingBuffer(const D3D12_RESOURCE_DESC& Desc, const void* pData)
{
	D3D12MA::ALLOCATION_DESC AllocDesc = {};
	AllocDesc.HeapType = D3D12_HEAP_TYPE_UPLOAD;
	ComPtr<ID3D12Resource> StagingResource;
	D3D12MA::Allocation* pAllocation = nullptr;

	pAllocator->CreateResource(&AllocDesc, &Desc, D3D12_RESOURCE_STATE_GENERIC_READ, nullptr, &pAllocation, IID_PPV_ARGS(StagingResource.ReleaseAndGetAddressOf()));

	BYTE* pMemory = nullptr;
	if (SUCCEEDED(StagingResource->Map(0, nullptr, reinterpret_cast<void**>(&pMemory))))
	{
		memcpy(pMemory, pData, Desc.Width);

		StagingResource->Unmap(0, nullptr);
	}

	StagingBuffer StagingBuffer = {};
	StagingBuffer.Buffer = std::move(StagingResource);
	StagingBuffer.pAllocation = pAllocation;
	return StagingBuffer;
}

void BufferManager::LoadMesh(Mesh& Mesh)
{
	StageVertexResource(Mesh);
	StageIndexResource(Mesh);

	Mesh.MeshletResource = pRenderDevice->InitializeRenderResourceHandle(RenderResourceType::Buffer, Mesh.Name + " [Meshlet Resource]");
	Mesh.VertexIndexResource = pRenderDevice->InitializeRenderResourceHandle(RenderResourceType::Buffer, Mesh.Name + " [Vertex Index Resource]");
	Mesh.PrimitiveIndexResource = pRenderDevice->InitializeRenderResourceHandle(RenderResourceType::Buffer, Mesh.Name + " [Primitive Index Resource]");

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