#include "pch.h"
#include "GpuBufferAllocator.h"
#include "RendererRegistry.h"

GpuBufferAllocator::GpuBufferAllocator(RenderDevice* pRenderDevice, size_t VertexBufferByteSize, size_t IndexBufferByteSize, size_t ConstantBufferByteSize)
	: pRenderDevice(pRenderDevice),
	m_BufferStrides{ sizeof(Vertex), sizeof(unsigned int), Math::AlignUp<UINT>(sizeof(ObjectConstants), D3D12_CONSTANT_BUFFER_DATA_PLACEMENT_ALIGNMENT) },
	m_Buffers{ { VertexBufferByteSize }, { IndexBufferByteSize } , { Math::AlignUp<UINT64>(ConstantBufferByteSize, D3D12_CONSTANT_BUFFER_DATA_PLACEMENT_ALIGNMENT)} }
{
	for (size_t i = 0; i < NumInternalBufferTypes; ++i)
	{
		size_t bufferByteSize = m_Buffers[i].Allocator.GetSize();
		RenderResourceHandle bufferHandle, uploadBufferHandle;

		if (i != ConstantBuffer)
		{
			bufferHandle = pRenderDevice->CreateBuffer([=](BufferProxy& proxy)
			{
				proxy.SetSizeInBytes(bufferByteSize);
				proxy.SetStride(m_BufferStrides[i]);
				proxy.InitialState = Resource::State::CopyDest;
			});
			m_Buffers[i].BufferHandle = bufferHandle;
			m_Buffers[i].pBuffer = pRenderDevice->GetBuffer(bufferHandle);
		}

		uploadBufferHandle = pRenderDevice->CreateBuffer([=](BufferProxy& proxy)
		{
			proxy.SetSizeInBytes(bufferByteSize);
			proxy.SetStride(m_BufferStrides[i]);
			proxy.SetCpuAccess(Buffer::CpuAccess::Write);
		});

		m_Buffers[i].UploadBufferHandle = uploadBufferHandle;
		m_Buffers[i].pUploadBuffer = pRenderDevice->GetBuffer(uploadBufferHandle);
		m_Buffers[i].pUploadBuffer->Map();
	}
	m_NumVertices = m_NumIndices = 0;
}

void GpuBufferAllocator::Stage(Scene& Scene, CommandContext* pCommandContext)
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

		StageVertex(vertices.data(), totalVertexBytes, pCommandContext);
		StageIndex(indices.data(), totalIndexBytes, pCommandContext);

		m_NumVertices += vertices.size();
		m_NumIndices += indices.size();
	}

	for (auto& model : Scene.Models)
	{
		UINT64 totalVertexBytes = model.Vertices.size() * sizeof(Vertex);
		UINT64 totalIndexBytes = model.Indices.size() * sizeof(UINT);

		auto vertexByteOffset = StageVertex(model.Vertices.data(), totalVertexBytes, pCommandContext);
		auto indexByteOffset = StageIndex(model.Indices.data(), totalIndexBytes, pCommandContext);

		for (auto& mesh : model.Meshes)
		{
			assert(vertexByteOffset % sizeof(Vertex) == 0 && "Vertex offset mismatch");
			assert(indexByteOffset % sizeof(UINT) == 0 && "Index offset mismatch");
			mesh.BaseVertexLocation += (vertexByteOffset / sizeof(Vertex));
			mesh.StartIndexLocation += (indexByteOffset / sizeof(UINT));
		}

		m_NumVertices += model.Vertices.size();
		m_NumIndices += model.Indices.size();

		if (!model.Path.empty())
		{
			CORE_INFO("{} Loaded", model.Path);
		}
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

	pCommandContext->TransitionBarrier(m_Buffers[VertexBuffer].pBuffer, Resource::State::VertexBuffer | Resource::State::NonPixelShaderResource);
	pCommandContext->TransitionBarrier(m_Buffers[IndexBuffer].pBuffer, Resource::State::IndexBuffer | Resource::State::NonPixelShaderResource);
	pCommandContext->FlushResourceBarriers();

	CreateBottomLevelAS(Scene, pCommandContext);
	CreateTopLevelAS(Scene, pCommandContext);
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

void GpuBufferAllocator::Bind(CommandContext* pCommandContext) const
{
	D3D12_VERTEX_BUFFER_VIEW vertexBufferView;
	vertexBufferView.BufferLocation = m_Buffers[VertexBuffer].pBuffer->GetGpuVirtualAddress();
	vertexBufferView.SizeInBytes = m_Buffers[VertexBuffer].Allocator.GetCurrentSize();
	vertexBufferView.StrideInBytes = sizeof(Vertex);
	pCommandContext->SetVertexBuffers(0, 1, &vertexBufferView);

	D3D12_INDEX_BUFFER_VIEW indexBufferView;
	indexBufferView.BufferLocation = m_Buffers[IndexBuffer].pBuffer->GetGpuVirtualAddress();
	indexBufferView.SizeInBytes = m_Buffers[IndexBuffer].Allocator.GetCurrentSize();
	indexBufferView.Format = DXGI_FORMAT_R32_UINT;
	pCommandContext->SetIndexBuffer(&indexBufferView);
}

size_t GpuBufferAllocator::StageVertex(const void* pData, size_t ByteSize, CommandContext* pCommandContext)
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
	pCommandContext->CopyBufferRegion(m_Buffers[VertexBuffer].pBuffer, vertexOffset, m_Buffers[VertexBuffer].pUploadBuffer, vertexOffset, vertexSize);
	return vertexOffset;
}

size_t GpuBufferAllocator::StageIndex(const void* pData, size_t ByteSize, CommandContext* pCommandContext)
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
	pCommandContext->CopyBufferRegion(m_Buffers[IndexBuffer].pBuffer, indexOffset, m_Buffers[IndexBuffer].pUploadBuffer, indexOffset, indexSize);
	return indexOffset;
}

void GpuBufferAllocator::CreateBottomLevelAS(Scene& Scene, CommandContext* pCommandContext)
{
	// we dont create a BLAS for skybox's box geometry
	UINT vertexOffset = Scene.Skybox.Mesh.VertexCount;
	UINT indexOffset = Scene.Skybox.Mesh.IndexCount;

	RaytracingGeometryDesc geometryDesc = {};
	geometryDesc.pVertexBuffer = m_Buffers[VertexBuffer].pBuffer;
	geometryDesc.VertexStride = sizeof(Vertex);
	geometryDesc.pIndexBuffer = m_Buffers[IndexBuffer].pBuffer;
	geometryDesc.IndexStride = sizeof(unsigned int);
	geometryDesc.IsOpaque = true;
	for (auto& model : Scene.Models)
	{
		RTBLAS rtblas;

		geometryDesc.NumVertices = model.Vertices.size();
		geometryDesc.VertexOffset = vertexOffset;

		geometryDesc.NumIndices = model.Indices.size();
		geometryDesc.IndexOffset = indexOffset;

		rtblas.BLAS.AddGeometry(geometryDesc);

		vertexOffset += model.Vertices.size();
		indexOffset += model.Indices.size();

		model.BottomLevelAccelerationStructureIndex = m_RaytracingBottomLevelAccelerationStructures.size();
		m_RaytracingBottomLevelAccelerationStructures.push_back(rtblas);
	}

	for (auto& rtblas : m_RaytracingBottomLevelAccelerationStructures)
	{
		UINT64 scratchSizeInBytes, resultSizeInBytes;
		rtblas.BLAS.ComputeMemoryRequirements(pRenderDevice->GetDevice(), false, &scratchSizeInBytes, &resultSizeInBytes);

		// BLAS Scratch
		rtblas.Handles.Scratch = pRenderDevice->CreateBuffer([=](BufferProxy& proxy)
		{
			proxy.SetSizeInBytes(scratchSizeInBytes);
			proxy.BindFlags = Resource::BindFlags::AccelerationStructure;
			proxy.InitialState = Resource::State::UnorderedAccess;
		});

		// BLAS Result
		rtblas.Handles.Result = pRenderDevice->CreateBuffer([=](BufferProxy& proxy)
		{
			proxy.SetSizeInBytes(resultSizeInBytes);
			proxy.BindFlags = Resource::BindFlags::AccelerationStructure;
			proxy.InitialState = Resource::State::AccelerationStructure;
		});

		Buffer* pResult = pRenderDevice->GetBuffer(rtblas.Handles.Result);
		Buffer* pScratch = pRenderDevice->GetBuffer(rtblas.Handles.Scratch);

		rtblas.Buffers.pScratch = pScratch;
		rtblas.Buffers.pResult = pResult;

		rtblas.BLAS.Generate(pCommandContext, pScratch, pResult);
	}
}

void GpuBufferAllocator::CreateTopLevelAS(Scene& Scene, CommandContext* pCommandContext)
{
	size_t i = 0;
	for (auto& model : Scene.Models)
	{
		RTBLAS& rtblas = m_RaytracingBottomLevelAccelerationStructures[model.BottomLevelAccelerationStructureIndex];
		Buffer* blas = pRenderDevice->GetBuffer(rtblas.Handles.Result);

		RaytracingInstanceDesc desc = {};
		desc.pBottomLevelAccelerationStructure = blas;
		desc.Transform = model.Transform.Matrix();
		desc.InstanceID = i;

		desc.HitGroupIndex = 0;

		m_RaytracingTopLevelAccelerationStructure.TLAS.AddInstance(desc);

		i++;
	}

	UINT64 scratchSizeInBytes, resultSizeInBytes, instanceDescsSizeInBytes;
	m_RaytracingTopLevelAccelerationStructure.TLAS.ComputeMemoryRequirements(pRenderDevice->GetDevice(), true, &scratchSizeInBytes, &resultSizeInBytes, &instanceDescsSizeInBytes);

	// TLAS Scratch
	m_RaytracingTopLevelAccelerationStructure.Handles.Scratch = pRenderDevice->CreateBuffer([=](BufferProxy& proxy)
	{
		proxy.SetSizeInBytes(scratchSizeInBytes);
		proxy.BindFlags = Resource::BindFlags::AccelerationStructure;
		proxy.InitialState = Resource::State::UnorderedAccess;
	});

	// TLAS Result
	m_RaytracingTopLevelAccelerationStructure.Handles.Result = pRenderDevice->CreateBuffer([=](BufferProxy& proxy)
	{
		proxy.SetSizeInBytes(resultSizeInBytes);
		proxy.BindFlags = Resource::BindFlags::AccelerationStructure;
		proxy.InitialState = Resource::State::AccelerationStructure;
	});

	m_RaytracingTopLevelAccelerationStructure.Handles.InstanceDescs = pRenderDevice->CreateBuffer([=](BufferProxy& proxy)
	{
		proxy.SetSizeInBytes(instanceDescsSizeInBytes);
		proxy.SetCpuAccess(Buffer::CpuAccess::Write);
	});

	Buffer* pScratch = pRenderDevice->GetBuffer(m_RaytracingTopLevelAccelerationStructure.Handles.Scratch);
	Buffer* pResult = pRenderDevice->GetBuffer(m_RaytracingTopLevelAccelerationStructure.Handles.Result);
	Buffer* pInstanceDescs = pRenderDevice->GetBuffer(m_RaytracingTopLevelAccelerationStructure.Handles.InstanceDescs);

	pScratch->SetDebugName(L"RTTLAS Scratch");
	pResult->SetDebugName(L"RTTLAS Result");

	m_RaytracingTopLevelAccelerationStructure.TLAS.Generate(pCommandContext, pScratch, pResult, pInstanceDescs);
}