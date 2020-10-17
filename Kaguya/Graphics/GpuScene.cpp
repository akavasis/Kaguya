#include "pch.h"
#include "GpuScene.h"
#include "RendererRegistry.h"

#define RAYTRACING_INSTANCEMASK_ALL 	(0xff)
#define RAYTRACING_INSTANCEMASK_OPAQUE 	(1 << 0)
#define RAYTRACING_INSTANCEMASK_LIGHT	(1 << 1)

struct HLSLMaterial
{
	float3	Albedo;
	float3	Emissive;
	float3	Specular;
	float3	Refraction;
	float	SpecularChance;
	float	Roughness;
	float	Fuzziness;
	float	IndexOfRefraction;
	uint	Model;

	int		TextureIndices[NumTextureTypes];
};

namespace
{
	static constexpr size_t NumLights					= 10000;
	static constexpr size_t NumMaterials				= 50000;

	static constexpr size_t LightBufferByteSize			= NumLights * sizeof(PolygonalLight);
	static constexpr size_t MaterialBufferByteSize		= NumMaterials * sizeof(HLSLMaterial);
	static constexpr size_t VertexBufferByteSize		= 50_MiB;
	static constexpr size_t IndexBufferByteSize			= 50_MiB;
	static constexpr size_t GeometryInfoBufferByteSize	= 50_MiB;
}

GpuScene::GpuScene(RenderDevice* pRenderDevice)
	: pRenderDevice(pRenderDevice),
	pScene(nullptr),
	GpuTextureAllocator(pRenderDevice)
{
	// Resource
	size_t resourceTablesMemorySizeInBytes[NumResources]	= { LightBufferByteSize, MaterialBufferByteSize, VertexBufferByteSize, IndexBufferByteSize, GeometryInfoBufferByteSize };
	size_t resourceTablesMemoryStrides[NumResources]		= { sizeof(PolygonalLight), sizeof(HLSLMaterial), sizeof(Vertex), sizeof(unsigned int), sizeof(GeometryInfo) };

	for (size_t i = 0; i < NumResources; ++i)
	{
		UploadResourceTables[i] = pRenderDevice->CreateDeviceBuffer([&](DeviceBufferProxy& proxy)
		{
			proxy.SetSizeInBytes(resourceTablesMemorySizeInBytes[i]);
			proxy.SetStride(resourceTablesMemoryStrides[i]);
			proxy.SetCpuAccess(DeviceBuffer::CpuAccess::Write);
		});
		ResourceTables[i] = pRenderDevice->CreateDeviceBuffer([&](DeviceBufferProxy& proxy)
		{
			proxy.SetSizeInBytes(resourceTablesMemorySizeInBytes[i]);
			proxy.SetStride(resourceTablesMemoryStrides[i]);
			proxy.InitialState = DeviceResource::State::CopyDest;
		});
		Allocators[i].Reset(resourceTablesMemorySizeInBytes[i]);
	}
}

void GpuScene::UploadLights()
{
	auto pUploadLightTable = pRenderDevice->GetBuffer(UploadResourceTables[LightTable]);

	size_t index = 0;

	std::vector<PolygonalLight> lights(pScene->Lights.size());
	for (auto iter : pScene->Lights)
	{
		lights[index++] = iter;
	}

	Upload(LightTable, lights.data(), lights.size() * sizeof(PolygonalLight), pUploadLightTable);
}

void GpuScene::UploadMaterials()
{
	auto pUploadMaterialTable = pRenderDevice->GetBuffer(UploadResourceTables[MaterialTable]);

	size_t index = 0;

	std::vector<HLSLMaterial> hlslMaterials;
	hlslMaterials.reserve(pScene->Materials.size());
	for (auto& material : pScene->Materials)
	{
		material.GpuMaterialIndex = index++;

		HLSLMaterial hlslMaterial = {};
		memcpy(&hlslMaterial, &material, sizeof(hlslMaterial));
		hlslMaterials.push_back(hlslMaterial);
	}

	Upload(MaterialTable, hlslMaterials.data(), hlslMaterials.size() * sizeof(HLSLMaterial), pUploadMaterialTable);
}

void GpuScene::UploadModels()
{
	auto pUploadVertexBuffer = pRenderDevice->GetBuffer(UploadResourceTables[VertexBuffer]);
	auto pUploadIndexBuffer = pRenderDevice->GetBuffer(UploadResourceTables[IndexBuffer]);

	auto pVertexBuffer = pRenderDevice->GetBuffer(ResourceTables[VertexBuffer]);
	auto pIndexBuffer = pRenderDevice->GetBuffer(ResourceTables[IndexBuffer]);

	// Upload skybox
	{
		auto skyboxModel = CreateBox(1.0f, 1.0f, 1.0f, 0);
		std::vector<Vertex> vertices = std::move(skyboxModel.Vertices);
		std::vector<UINT> indices = std::move(skyboxModel.Indices);

		pScene->Skybox.Mesh.BoundingBox = skyboxModel.BoundingBox;
		pScene->Skybox.Mesh.IndexCount = indices.size();
		pScene->Skybox.Mesh.StartIndexLocation = 0;
		pScene->Skybox.Mesh.VertexCount = vertices.size();
		pScene->Skybox.Mesh.BaseVertexLocation = 0;

		UINT64 totalVertexBytes = vertices.size() * sizeof(Vertex);
		UINT64 totalIndexBytes = indices.size() * sizeof(UINT);

		Upload(VertexBuffer, vertices.data(), totalVertexBytes, pUploadVertexBuffer);
		Upload(IndexBuffer, indices.data(), totalIndexBytes, pUploadIndexBuffer);
	}

	for (auto& model : pScene->Models)
	{
		UINT64 totalVertexBytes = model.Vertices.size() * sizeof(Vertex);
		UINT64 totalIndexBytes = model.Indices.size() * sizeof(UINT);

		auto vertexByteOffset = Upload(VertexBuffer, model.Vertices.data(), totalVertexBytes, pUploadVertexBuffer);
		auto indexByteOffset = Upload(IndexBuffer, model.Indices.data(), totalIndexBytes, pUploadIndexBuffer);

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

		// Add model's meshes into RTBLAS
		RTBLAS rtblas;
		for (auto& mesh : model.Meshes)
		{
			// Update mesh's BLAS Index
			mesh.BottomLevelAccelerationStructureIndex	= m_RaytracingBottomLevelAccelerationStructures.size();

			RaytracingGeometryDesc geometryDesc = {};
			geometryDesc.pVertexBuffer			= pVertexBuffer;
			geometryDesc.VertexStride			= sizeof(Vertex);
			geometryDesc.pIndexBuffer			= pIndexBuffer;
			geometryDesc.IndexStride			= sizeof(unsigned int);
			geometryDesc.IsOpaque				= true;
			geometryDesc.NumVertices			= mesh.VertexCount;
			geometryDesc.VertexOffset			= mesh.BaseVertexLocation;
			geometryDesc.NumIndices				= mesh.IndexCount;
			geometryDesc.IndexOffset			= mesh.StartIndexLocation;

			rtblas.BLAS.AddGeometry(geometryDesc);
		}
		m_RaytracingBottomLevelAccelerationStructures.push_back(rtblas);
	}
}

void GpuScene::UploadModelInstances()
{
	auto pUploadGeometryInfoTable = pRenderDevice->GetBuffer(UploadResourceTables[GeometryInfoTable]);

	size_t instanceID = 0;
	std::vector<GeometryInfo> geometryInfos;
	for (auto& modelInstance : pScene->ModelInstances)
	{
		for (auto& meshInstance : modelInstance.MeshInstances)
		{
			meshInstance.InstanceID = instanceID++;

			GeometryInfo info;
			info.VertexOffset = meshInstance.pMesh->BaseVertexLocation;
			info.IndexOffset = meshInstance.pMesh->StartIndexLocation;
			info.MaterialIndex = modelInstance.pMaterial->GpuMaterialIndex;
			XMStoreFloat4x4(&info.World, XMMatrixTranspose(meshInstance.Transform.Matrix()));

			geometryInfos.push_back(info);
		}
	}

	Upload(GeometryInfoTable, geometryInfos.data(), geometryInfos.size() * sizeof(GeometryInfo), pUploadGeometryInfoTable);
}

void GpuScene::Commit(RenderContext& RenderContext)
{
	for (size_t i = 0; i < NumResources; ++i)
	{
		auto pUploadBuffer = pRenderDevice->GetBuffer(UploadResourceTables[i]);
		auto pBuffer = pRenderDevice->GetBuffer(ResourceTables[i]);

		RenderContext->CopyResource(pBuffer, pUploadBuffer);

		switch (i)
		{
		case MaterialTable:
			RenderContext->TransitionBarrier(pBuffer, DeviceResource::State::PixelShaderResource | DeviceResource::State::NonPixelShaderResource);
			break;

		case VertexBuffer:
			RenderContext->TransitionBarrier(pBuffer, DeviceResource::State::VertexBuffer | DeviceResource::State::NonPixelShaderResource);
			break;

		case IndexBuffer:
			RenderContext->TransitionBarrier(pBuffer, DeviceResource::State::IndexBuffer | DeviceResource::State::NonPixelShaderResource);
			break;

		case GeometryInfoTable:
			RenderContext->TransitionBarrier(pBuffer, DeviceResource::State::PixelShaderResource | DeviceResource::State::NonPixelShaderResource);
			break;
		}
	}
	RenderContext->FlushResourceBarriers();

	CreateBottomLevelAS(RenderContext);
	CreateTopLevelAS(RenderContext);

	auto pVertexBuffer = pRenderDevice->GetBuffer(ResourceTables[VertexBuffer]);
	auto pIndexBuffer = pRenderDevice->GetBuffer(ResourceTables[IndexBuffer]);

	VertexBufferView.BufferLocation = pVertexBuffer->GetGpuVirtualAddress();
	VertexBufferView.SizeInBytes = Allocators[VertexBuffer].GetCurrentSize();
	VertexBufferView.StrideInBytes = sizeof(Vertex);
	RenderContext->SetVertexBuffers(0, 1, &VertexBufferView);

	IndexBufferView.BufferLocation = pIndexBuffer->GetGpuVirtualAddress();
	IndexBufferView.SizeInBytes = Allocators[IndexBuffer].GetCurrentSize();
	IndexBufferView.Format = DXGI_FORMAT_R32_UINT;
	RenderContext->SetIndexBuffer(&IndexBufferView);

	GpuTextureAllocator.Stage(*pScene, RenderContext);
}

void GpuScene::DisposeResources()
{
	for (auto& rtblas : m_RaytracingBottomLevelAccelerationStructures)
	{
		pRenderDevice->Destroy(&rtblas.Handles.Scratch);
	}

	pRenderDevice->Destroy(&m_RaytracingTopLevelAccelerationStructure.Handles.Scratch);
	pRenderDevice->Destroy(&m_RaytracingTopLevelAccelerationStructure.Handles.InstanceDescs);

	GpuTextureAllocator.DisposeResources();
}

void GpuScene::Update(float AspectRatio)
{
	pScene->Camera.SetAspectRatio(AspectRatio);
}

size_t GpuScene::Upload(EResource Type, const void* pData, size_t ByteSize, DeviceBuffer* pUploadBuffer)
{
	if (pData && ByteSize != 0)
	{
		auto pair = Allocators[Type].Allocate(ByteSize);
		assert(pair.has_value() && "Unable to allocate data, consider increasing memory");
		auto [offset, size] = pair.value();

		// Stage vertex
		auto pGPU = pUploadBuffer->Map();
		auto pCPU = pData;
		memcpy(&pGPU[offset], pCPU, size);
		return offset;
	}
	return 0;
}

void GpuScene::CreateBottomLevelAS(RenderContext& RenderContext)
{
	PIXMarker(RenderContext->GetD3DCommandList(), L"Bottom Level AS Generation");

	for (auto& rtblas : m_RaytracingBottomLevelAccelerationStructures)
	{
		UINT64 scratchSizeInBytes, resultSizeInBytes;
		rtblas.BLAS.ComputeMemoryRequirements(&pRenderDevice->Device, false, &scratchSizeInBytes, &resultSizeInBytes);

		// BLAS Scratch
		rtblas.Handles.Scratch = pRenderDevice->CreateDeviceBuffer([=](DeviceBufferProxy& proxy)
		{
			proxy.SetSizeInBytes(scratchSizeInBytes);
			proxy.BindFlags = DeviceResource::BindFlags::AccelerationStructure;
			proxy.InitialState = DeviceResource::State::UnorderedAccess;
		});

		// BLAS Result
		rtblas.Handles.Result = pRenderDevice->CreateDeviceBuffer([=](DeviceBufferProxy& proxy)
		{
			proxy.SetSizeInBytes(resultSizeInBytes);
			proxy.BindFlags = DeviceResource::BindFlags::AccelerationStructure;
			proxy.InitialState = DeviceResource::State::AccelerationStructure;
		});

		DeviceBuffer* pResult = pRenderDevice->GetBuffer(rtblas.Handles.Result);
		DeviceBuffer* pScratch = pRenderDevice->GetBuffer(rtblas.Handles.Scratch);

		rtblas.BLAS.Generate(RenderContext.GetCommandContext(), pScratch, pResult);
	}
}

void GpuScene::CreateTopLevelAS(RenderContext& RenderContext)
{
	PIXMarker(RenderContext->GetD3DCommandList(), L"Top Level AS Generation");

	size_t hitGroupIndex = 0;
	for (const auto& modelInstance : pScene->ModelInstances)
	{
		for (const auto& meshInstance : modelInstance.MeshInstances)
		{
			RTBLAS& rtblas = m_RaytracingBottomLevelAccelerationStructures[meshInstance.pMesh->BottomLevelAccelerationStructureIndex];
			DeviceBuffer* blas = pRenderDevice->GetBuffer(rtblas.Handles.Result);

			RaytracingInstanceDesc desc = {};
			desc.AccelerationStructure = blas->GetGpuVirtualAddress();
			desc.Transform = meshInstance.Transform.Matrix();
			desc.InstanceID = meshInstance.InstanceID;
			desc.InstanceMask = RAYTRACING_INSTANCEMASK_OPAQUE;

			desc.HitGroupIndex = hitGroupIndex;

			m_RaytracingTopLevelAccelerationStructure.TLAS.AddInstance(desc);

			hitGroupIndex++;
		}
	}

	UINT64 scratchSizeInBytes, resultSizeInBytes, instanceDescsSizeInBytes;
	m_RaytracingTopLevelAccelerationStructure.TLAS.ComputeMemoryRequirements(&pRenderDevice->Device, true, &scratchSizeInBytes, &resultSizeInBytes, &instanceDescsSizeInBytes);

	// TLAS Scratch
	m_RaytracingTopLevelAccelerationStructure.Handles.Scratch = pRenderDevice->CreateDeviceBuffer([=](DeviceBufferProxy& proxy)
	{
		proxy.SetSizeInBytes(scratchSizeInBytes);
		proxy.BindFlags = DeviceResource::BindFlags::AccelerationStructure;
		proxy.InitialState = DeviceResource::State::UnorderedAccess;
	});

	// TLAS Result
	m_RaytracingTopLevelAccelerationStructure.Handles.Result = pRenderDevice->CreateDeviceBuffer([=](DeviceBufferProxy& proxy)
	{
		proxy.SetSizeInBytes(resultSizeInBytes);
		proxy.BindFlags = DeviceResource::BindFlags::AccelerationStructure;
		proxy.InitialState = DeviceResource::State::AccelerationStructure;
	});

	m_RaytracingTopLevelAccelerationStructure.Handles.InstanceDescs = pRenderDevice->CreateDeviceBuffer([=](DeviceBufferProxy& proxy)
	{
		proxy.SetSizeInBytes(instanceDescsSizeInBytes);
		proxy.SetCpuAccess(DeviceBuffer::CpuAccess::Write);
	});

	DeviceBuffer* pScratch = pRenderDevice->GetBuffer(m_RaytracingTopLevelAccelerationStructure.Handles.Scratch);
	DeviceBuffer* pResult = pRenderDevice->GetBuffer(m_RaytracingTopLevelAccelerationStructure.Handles.Result);
	DeviceBuffer* pInstanceDescs = pRenderDevice->GetBuffer(m_RaytracingTopLevelAccelerationStructure.Handles.InstanceDescs);

	pResult->SetDebugName(L"RTTLAS Result");

	m_RaytracingTopLevelAccelerationStructure.TLAS.Generate(RenderContext.GetCommandContext(), pScratch, pResult, pInstanceDescs);
}