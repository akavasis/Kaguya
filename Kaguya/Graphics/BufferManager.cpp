#include "pch.h"
#include "BufferManager.h"

BufferManager::BufferManager(RenderDevice* pRenderDevice)
	: pRenderDevice(pRenderDevice)
{

}

void BufferManager::Stage(Scene& Scene, RenderContext& RenderContext)
{
	for (auto& model : Scene.Models)
	{
		LoadModel(model);
	}

	for (auto& [handle, stagingBuffer] : m_UnstagedBuffers)
	{
		StageBuffer(handle, stagingBuffer, RenderContext);
	}
	RenderContext->FlushResourceBarriers();
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

void BufferManager::LoadModel(Model& Model)
{
	StageVertexResource(Model);
	StageIndexResource(Model);

	Model.MeshletHeap = pRenderDevice->InitializeRenderResourceHandle(RenderResourceType::Heap, Model.Name + " [Meshlet Heap]");
	Model.VertexIndexHeap = pRenderDevice->InitializeRenderResourceHandle(RenderResourceType::Heap, Model.Name + " [Vertex Index Heap]");
	Model.PrimitiveIndexHeap = pRenderDevice->InitializeRenderResourceHandle(RenderResourceType::Heap, Model.Name + " [Primitive Index Heap]");

	std::vector<D3D12_RESOURCE_DESC> MeshletDescs;
	std::vector<D3D12_RESOURCE_DESC> VertexIndexDescs;
	std::vector<D3D12_RESOURCE_DESC> PrimitiveIndexDescs;

	for (const auto& mesh : Model)
	{
		UINT64 MeshletResourceSizeInBytes = mesh.Meshlets.size() * sizeof(Meshlet);
		UINT64 VertexIndexResourceSizeInBytes = mesh.VertexIndices.size() * sizeof(uint32_t);
		UINT64 PrimitiveIndexResourceSizeInBytes = mesh.PrimitiveIndices.size() * sizeof(MeshletPrimitive);

		MeshletDescs.push_back(CD3DX12_RESOURCE_DESC::Buffer(MeshletResourceSizeInBytes));
		VertexIndexDescs.push_back(CD3DX12_RESOURCE_DESC::Buffer(VertexIndexResourceSizeInBytes));
		PrimitiveIndexDescs.push_back(CD3DX12_RESOURCE_DESC::Buffer(PrimitiveIndexResourceSizeInBytes));
	}

	// Create MeshletHeap and MeshletResource
	{
		std::vector<D3D12_RESOURCE_ALLOCATION_INFO1> ResourceAllocationInfo1(MeshletDescs.size());
		auto ResourceAllocationInfo = pRenderDevice->Device.GetApiHandle()->GetResourceAllocationInfo1(0, MeshletDescs.size(), MeshletDescs.data(), ResourceAllocationInfo1.data());

		auto HeapDesc = CD3DX12_HEAP_DESC(ResourceAllocationInfo.SizeInBytes, D3D12_HEAP_TYPE_DEFAULT, 0, D3D12_HEAP_FLAG_ALLOW_ONLY_BUFFERS);
		pRenderDevice->CreateHeap(Model.MeshletHeap, HeapDesc);

		for (size_t i = 0; i < Model.Meshes.size(); ++i)
		{
			auto& mesh = Model.Meshes[i];

			mesh.MeshletResource = pRenderDevice->InitializeRenderResourceHandle(RenderResourceType::Buffer, Model.Name + " Mesh[" + std::to_string(i) + "]" + " [Meshlet Resource]");

			UINT64 HeapOffset = ResourceAllocationInfo1[i].Offset;
			UINT64 SizeInBytes = ResourceAllocationInfo1[i].SizeInBytes;
			pRenderDevice->CreateBuffer(mesh.MeshletResource, Model.MeshletHeap, HeapOffset, [&](BufferProxy& proxy)
			{
				proxy.SetSizeInBytes(SizeInBytes);
				proxy.SetStride(sizeof(Meshlet));
				proxy.InitialState = Resource::State::CopyDest;
			});

			m_UnstagedBuffers[mesh.MeshletResource] = CreateStagingBuffer(MeshletDescs[i], mesh.Meshlets.data());
		}
	}

	// Create VertexIndexHeap and VertexIndexResource
	{
		std::vector<D3D12_RESOURCE_ALLOCATION_INFO1> ResourceAllocationInfo1(VertexIndexDescs.size());
		auto ResourceAllocationInfo = pRenderDevice->Device.GetApiHandle()->GetResourceAllocationInfo1(0, VertexIndexDescs.size(), VertexIndexDescs.data(), ResourceAllocationInfo1.data());

		auto HeapDesc = CD3DX12_HEAP_DESC(ResourceAllocationInfo.SizeInBytes, D3D12_HEAP_TYPE_DEFAULT, 0, D3D12_HEAP_FLAG_ALLOW_ONLY_BUFFERS);
		pRenderDevice->CreateHeap(Model.VertexIndexHeap, HeapDesc);

		for (size_t i = 0; i < Model.Meshes.size(); ++i)
		{
			auto& mesh = Model.Meshes[i];

			mesh.VertexIndexResource = pRenderDevice->InitializeRenderResourceHandle(RenderResourceType::Buffer, Model.Name + " Mesh[" + std::to_string(i) + "]" + " [Vertex Index Resource]");

			UINT64 HeapOffset = ResourceAllocationInfo1[i].Offset;
			UINT64 SizeInBytes = ResourceAllocationInfo1[i].SizeInBytes;
			pRenderDevice->CreateBuffer(mesh.VertexIndexResource, Model.VertexIndexHeap, HeapOffset, [&](BufferProxy& proxy)
			{
				proxy.SetSizeInBytes(SizeInBytes);
				proxy.SetStride(sizeof(uint32_t));
				proxy.InitialState = Resource::State::CopyDest;
			});

			m_UnstagedBuffers[mesh.VertexIndexResource] = CreateStagingBuffer(VertexIndexDescs[i], mesh.VertexIndices.data());
		}
	}

	// Create PrimitiveIndexHeap and PrimitiveIndexResource
	{
		std::vector<D3D12_RESOURCE_ALLOCATION_INFO1> ResourceAllocationInfo1(PrimitiveIndexDescs.size());
		auto ResourceAllocationInfo = pRenderDevice->Device.GetApiHandle()->GetResourceAllocationInfo1(0, PrimitiveIndexDescs.size(), PrimitiveIndexDescs.data(), ResourceAllocationInfo1.data());

		auto HeapDesc = CD3DX12_HEAP_DESC(ResourceAllocationInfo.SizeInBytes, D3D12_HEAP_TYPE_DEFAULT, 0, D3D12_HEAP_FLAG_ALLOW_ONLY_BUFFERS);
		pRenderDevice->CreateHeap(Model.PrimitiveIndexHeap, HeapDesc);

		for (size_t i = 0; i < Model.Meshes.size(); ++i)
		{
			auto& mesh = Model.Meshes[i];

			mesh.PrimitiveIndexResource = pRenderDevice->InitializeRenderResourceHandle(RenderResourceType::Buffer, Model.Name + " Mesh[" + std::to_string(i) + "]" + " [Primitive Index Resource]");

			UINT64 HeapOffset = ResourceAllocationInfo1[i].Offset;
			UINT64 SizeInBytes = ResourceAllocationInfo1[i].SizeInBytes;
			pRenderDevice->CreateBuffer(mesh.PrimitiveIndexResource, Model.PrimitiveIndexHeap, HeapOffset, [&](BufferProxy& proxy)
			{
				proxy.SetSizeInBytes(SizeInBytes);
				proxy.SetStride(sizeof(MeshletPrimitive));
				proxy.InitialState = Resource::State::CopyDest;
			});

			m_UnstagedBuffers[mesh.PrimitiveIndexResource] = CreateStagingBuffer(PrimitiveIndexDescs[i], mesh.PrimitiveIndices.data());
		}
	}
}

void BufferManager::StageVertexResource(Model& Model)
{
	Model.VertexResource = pRenderDevice->InitializeRenderResourceHandle(RenderResourceType::Buffer, Model.Name + "[Vertex Resource]");
	UINT64 VertexResourceSizeInBytes = Model.Vertices.size() * sizeof(Vertex);

	pRenderDevice->CreateBuffer(Model.VertexResource, [=](BufferProxy& proxy)
	{
		proxy.SetSizeInBytes(VertexResourceSizeInBytes);
		proxy.SetStride(sizeof(Vertex));
		proxy.InitialState = Resource::State::CopyDest;
	});

	Buffer* pBuffer = pRenderDevice->GetBuffer(Model.VertexResource);
	m_UnstagedBuffers[Model.VertexResource] = CreateStagingBuffer(pBuffer->GetApiHandle()->GetDesc(), Model.Vertices.data());
}

void BufferManager::StageIndexResource(Model& Model)
{
	Model.IndexResource = pRenderDevice->InitializeRenderResourceHandle(RenderResourceType::Buffer, Model.Name + "[Index Resource]");
	UINT64 IndexResourceSizeInBytes = Model.Indices.size() * sizeof(uint32_t);

	pRenderDevice->CreateBuffer(Model.IndexResource, [=](BufferProxy& proxy)
	{
		proxy.SetSizeInBytes(IndexResourceSizeInBytes);
		proxy.SetStride(sizeof(uint32_t));
		proxy.InitialState = Resource::State::CopyDest;
	});

	Buffer* pBuffer = pRenderDevice->GetBuffer(Model.IndexResource);
	m_UnstagedBuffers[Model.IndexResource] = CreateStagingBuffer(pBuffer->GetApiHandle()->GetDesc(), Model.Indices.data());
}

void BufferManager::StageBuffer(RenderResourceHandle Handle, StagingBuffer& StagingBuffer, RenderContext& RenderContext)
{
	Buffer* pBuffer = pRenderDevice->GetBuffer(Handle);
	Buffer* pUploadBuffer = &StagingBuffer.Buffer;

	RenderContext->CopyBufferRegion(pBuffer, 0, pUploadBuffer, 0, pUploadBuffer->GetMemoryRequested());

	RenderContext->TransitionBarrier(pBuffer, Resource::State::PixelShaderResource | Resource::State::NonPixelShaderResource);
}