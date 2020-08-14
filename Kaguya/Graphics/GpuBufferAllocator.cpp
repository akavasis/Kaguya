#include "pch.h"
#include "GpuBufferAllocator.h"
#include "RendererRegistry.h"

GpuBufferAllocator::GpuBufferAllocator(RenderDevice* pRenderDevice, size_t VertexBufferByteSize, size_t IndexBufferByteSize, size_t ConstantBufferByteSize, size_t GeometryInfoBufferByteSize)
	: pRenderDevice(pRenderDevice),
	m_Buffers{
	{ VertexBufferByteSize },
	{ IndexBufferByteSize },
	{ Math::AlignUp<UINT64>(ConstantBufferByteSize, D3D12_CONSTANT_BUFFER_DATA_PLACEMENT_ALIGNMENT)},
	{ GeometryInfoBufferByteSize } }
{
	UINT strides[NumInternalBufferTypes] = { sizeof(Vertex), sizeof(unsigned int), Math::AlignUp<UINT>(sizeof(ObjectConstants), D3D12_CONSTANT_BUFFER_DATA_PLACEMENT_ALIGNMENT), sizeof(GeometryInfo) };

	for (size_t i = 0; i < NumInternalBufferTypes; ++i)
	{
		size_t bufferByteSize = m_Buffers[i].Allocator.GetSize();
		RenderResourceHandle bufferHandle, uploadBufferHandle;

		if (i != ConstantBuffer)
		{
			bufferHandle = pRenderDevice->CreateBuffer([=](BufferProxy& proxy)
			{
				proxy.SetSizeInBytes(bufferByteSize);
				proxy.SetStride(strides[i]);
				proxy.InitialState = Resource::State::CopyDest;
			});
			m_Buffers[i].BufferHandle = bufferHandle;
			m_Buffers[i].pBuffer = pRenderDevice->GetBuffer(bufferHandle);
		}

		uploadBufferHandle = pRenderDevice->CreateBuffer([=](BufferProxy& proxy)
		{
			proxy.SetSizeInBytes(bufferByteSize);
			proxy.SetStride(strides[i]);
			proxy.SetCpuAccess(Buffer::CpuAccess::Write);
		});

		m_Buffers[i].UploadBufferHandle = uploadBufferHandle;
		m_Buffers[i].pUploadBuffer = pRenderDevice->GetBuffer(uploadBufferHandle);
		m_Buffers[i].pUploadBuffer->Map();
	}
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

	for (auto& model : Scene.Models)
	{
		UINT64 totalGeometryInfoBytes = model.Meshes.size() * m_Buffers[GeometryInfoBuffer].pUploadBuffer->GetStride();
		auto pair = m_Buffers[GeometryInfoBuffer].Allocate(totalGeometryInfoBytes);
		if (!pair)
		{
			CORE_WARN("{} : Unable to allocate geometry info data, consider increasing memory", __FUNCTION__);
			assert(false);
		}
		auto [geometryInfoOffset, geometryInfoSize] = pair.value();
		std::vector<GeometryInfo> infos;

		for (auto& mesh : model.Meshes)
		{
			GeometryInfo info;
			info.VertexOffset = mesh.BaseVertexLocation;
			info.IndexOffset = mesh.StartIndexLocation;
			info.MaterialIndex = mesh.MaterialIndex;

			infos.push_back(info);
		}

		auto pGPU = m_Buffers[GeometryInfoBuffer].pUploadBuffer->Map();
		void* pCPU = infos.data();
		memcpy(&pGPU[geometryInfoOffset], pCPU, totalGeometryInfoBytes);
		pCommandContext->CopyBufferRegion(m_Buffers[GeometryInfoBuffer].pBuffer, geometryInfoOffset, m_Buffers[GeometryInfoBuffer].pUploadBuffer, geometryInfoOffset, geometryInfoSize);
	}

	pCommandContext->TransitionBarrier(m_Buffers[VertexBuffer].pBuffer, Resource::State::VertexBuffer | Resource::State::NonPixelShaderResource);
	pCommandContext->TransitionBarrier(m_Buffers[IndexBuffer].pBuffer, Resource::State::IndexBuffer | Resource::State::NonPixelShaderResource);
	pCommandContext->FlushResourceBarriers();

	CreateBottomLevelAS(Scene, pCommandContext);
	CreateTopLevelAS(Scene, pCommandContext);
	CreateShaderTableBuffers(Scene);
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
	RaytracingGeometryDesc geometryDesc = {};
	geometryDesc.pVertexBuffer = m_Buffers[VertexBuffer].pBuffer;
	geometryDesc.VertexStride = sizeof(Vertex);
	geometryDesc.pIndexBuffer = m_Buffers[IndexBuffer].pBuffer;
	geometryDesc.IndexStride = sizeof(unsigned int);
	geometryDesc.IsOpaque = true;
	for (auto& model : Scene.Models)
	{
		for (auto& mesh : model.Meshes)
		{
			RTBLAS rtblas;

			geometryDesc.NumVertices = mesh.VertexCount;
			geometryDesc.VertexOffset = mesh.BaseVertexLocation;

			geometryDesc.NumIndices = mesh.IndexCount;
			geometryDesc.IndexOffset = mesh.StartIndexLocation;

			rtblas.BLAS.AddGeometry(geometryDesc);

			mesh.BottomLevelAccelerationStructureIndex = m_RaytracingBottomLevelAccelerationStructures.size();
			m_RaytracingBottomLevelAccelerationStructures.push_back(rtblas);
		}
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
	size_t instanceID = 0;
	size_t hitGroupIndex = 0;
	for (const auto& model : Scene.Models)
	{
		for (const auto& mesh : model.Meshes)
		{
			RTBLAS& rtblas = m_RaytracingBottomLevelAccelerationStructures[mesh.BottomLevelAccelerationStructureIndex];
			Buffer* blas = pRenderDevice->GetBuffer(rtblas.Handles.Result);

			RaytracingInstanceDesc desc = {};
			desc.AccelerationStructure = blas->GetGpuVirtualAddress();
			desc.Transform = mesh.Transform.Matrix();
			desc.InstanceID = instanceID;

			desc.HitGroupIndex = hitGroupIndex;

			m_RaytracingTopLevelAccelerationStructure.TLAS.AddInstance(desc);

			instanceID++;
			hitGroupIndex += 2;
		}
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

void GpuBufferAllocator::CreateShaderTableBuffers(Scene& Scene)
{
	RaytracingPipelineState* pRaytracingPipelineState = pRenderDevice->GetRaytracingPSO(RaytracingPSOs::Raytracing);

	// Ray Generation Shader Table
	{
		ShaderTable<void> shaderTable;
		shaderTable.AddShaderRecord(pRaytracingPipelineState->GetShaderIdentifier(L"RayGen"));

		UINT64 shaderTableSizeInBytes; UINT stride;
		shaderTable.ComputeMemoryRequirements(&shaderTableSizeInBytes);
		stride = shaderTable.GetShaderRecordStride();

		m_RayGenerationShaderTable = pRenderDevice->CreateBuffer([shaderTableSizeInBytes, stride](BufferProxy& proxy)
		{
			proxy.SetSizeInBytes(shaderTableSizeInBytes);
			proxy.SetStride(stride);
			proxy.SetCpuAccess(Buffer::CpuAccess::Write);
		});

		Buffer* pShaderTableBuffer = pRenderDevice->GetBuffer(m_RayGenerationShaderTable);
		shaderTable.Generate(pShaderTableBuffer);
	}

	// Miss Shader Table
	{
		ShaderTable<void> shaderTable;
		shaderTable.AddShaderRecord(pRaytracingPipelineState->GetShaderIdentifier(L"Miss"));
		shaderTable.AddShaderRecord(pRaytracingPipelineState->GetShaderIdentifier(L"ShadowMiss"));

		UINT64 shaderTableSizeInBytes; UINT stride;
		shaderTable.ComputeMemoryRequirements(&shaderTableSizeInBytes);
		stride = shaderTable.GetShaderRecordStride();

		m_MissShaderTable = pRenderDevice->CreateBuffer([shaderTableSizeInBytes, stride](BufferProxy& proxy)
		{
			proxy.SetSizeInBytes(shaderTableSizeInBytes);
			proxy.SetStride(stride);
			proxy.SetCpuAccess(Buffer::CpuAccess::Write);
		});

		Buffer* pShaderTableBuffer = pRenderDevice->GetBuffer(m_MissShaderTable);
		shaderTable.Generate(pShaderTableBuffer);
	}

	// Hit Group Shader Table
	{
		struct HitGroupShaderRootArguments
		{
			unsigned int GeometryIndex; // Constants
		};

		ShaderTable<HitGroupShaderRootArguments> shaderTable;

		ShaderIdentifier hitGroupSID = pRaytracingPipelineState->GetShaderIdentifier(L"Default");
		ShaderIdentifier shadowHitGroupSID = pRaytracingPipelineState->GetShaderIdentifier(L"Shadow");

		size_t i = 0;
		for (const auto& model : Scene.Models)
		{
			for (const auto& mesh : model.Meshes)
			{
				HitGroupShaderRootArguments hitInfo = {};
				hitInfo.GeometryIndex = i;

				shaderTable.AddShaderRecord(hitGroupSID, hitInfo);
				shaderTable.AddShaderRecord(shadowHitGroupSID, hitInfo);

				i++;
			}
		}

		UINT64 shaderTableSizeInBytes; UINT stride;
		shaderTable.ComputeMemoryRequirements(&shaderTableSizeInBytes);
		stride = shaderTable.GetShaderRecordStride();

		m_HitGroupShaderTable = pRenderDevice->CreateBuffer([shaderTableSizeInBytes, stride](BufferProxy& proxy)
		{
			proxy.SetSizeInBytes(shaderTableSizeInBytes);
			proxy.SetStride(stride);
			proxy.SetCpuAccess(Buffer::CpuAccess::Write);
		});

		Buffer* pShaderTableBuffer = pRenderDevice->GetBuffer(m_HitGroupShaderTable);
		shaderTable.Generate(pShaderTableBuffer);
	}
}