#include "pch.h"
#include "GpuScene.h"
#include "RendererRegistry.h"

#define RAYTRACING_INSTANCEMASK_ALL 	(0xff)
#define RAYTRACING_INSTANCEMASK_OPAQUE 	(1 << 0)
#define RAYTRACING_INSTANCEMASK_LIGHT	(1 << 1)

struct HLSLPolygonalLight
{
	matrix	World;
	float3	Color;
	float	Intensity;
	float	Width;
	float	Height;
};

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
	static constexpr size_t NumLights					= 1000;
	static constexpr size_t NumMaterials				= 1000;

	static constexpr size_t LightBufferByteSize			= NumLights * sizeof(HLSLPolygonalLight);
	static constexpr size_t MaterialBufferByteSize		= NumMaterials * sizeof(HLSLMaterial);
	static constexpr size_t VertexBufferByteSize		= 30_MiB;
	static constexpr size_t IndexBufferByteSize			= 30_MiB;
	static constexpr size_t GeometryInfoBufferByteSize	= 30_MiB;
}

HLSLPolygonalLight GetShaderLightDesc(const PolygonalLight& Light)
{
	matrix World; XMStoreFloat4x4(&World, XMMatrixTranspose(Light.Transform.Matrix()));
	return {
		.World		= World,
		.Color		= Light.Color,
		.Intensity	= Light.Intensity,
		.Width		= Light.Width,
		.Height		= Light.Height
	};
}

HLSLMaterial GetShaderMaterialDesc(const Material& Material)
{
	return {
		.Albedo				= Material.Albedo,
		.Emissive			= Material.Emissive,
		.Specular			= Material.Specular,
		.Refraction			= Material.Refraction,
		.SpecularChance		= Material.SpecularChance,
		.Roughness			= Material.Roughness,
		.Fuzziness			= Material.Fuzziness,
		.IndexOfRefraction	= Material.IndexOfRefraction,
		.Model				= Material.Model,
		.TextureIndices		= { 
			Material.TextureIndices[0],
			Material.TextureIndices[1],
			Material.TextureIndices[2],
			Material.TextureIndices[3],
			Material.TextureIndices[4]
		}
	};
}

GeometryInfo GetShaderMeshDesc(size_t MaterialIndex, const MeshInstance& MeshInstance)
{
	matrix World; XMStoreFloat4x4(&World, XMMatrixTranspose(MeshInstance.Transform.Matrix()));
	return {
		.VertexOffset = MeshInstance.pMesh->BaseVertexLocation,
		.IndexOffset = MeshInstance.pMesh->StartIndexLocation,
		.MaterialIndex = (uint)MaterialIndex,
		.World = World
	};
}

GpuScene::GpuScene(RenderDevice* pRenderDevice)
	: SV_pRenderDevice(pRenderDevice),
	pScene(nullptr),
	GpuTextureAllocator(pRenderDevice)
{
	// Resource
	size_t resourceTablesMemorySizeInBytes[NumResources] = { LightBufferByteSize, MaterialBufferByteSize, VertexBufferByteSize, IndexBufferByteSize, GeometryInfoBufferByteSize };
	size_t resourceTablesMemoryStrides[NumResources] = { sizeof(HLSLPolygonalLight), sizeof(HLSLMaterial), sizeof(Vertex), sizeof(unsigned int), sizeof(GeometryInfo) };

	for (size_t i = 0; i < NumResources; ++i)
	{
		m_UploadResourceTables[i] = pRenderDevice->CreateDeviceBuffer([&](DeviceBufferProxy& proxy)
		{
			proxy.SetSizeInBytes(resourceTablesMemorySizeInBytes[i]);
			proxy.SetStride(resourceTablesMemoryStrides[i]);
			proxy.SetCpuAccess(DeviceBuffer::CpuAccess::Write);
		});
		m_ResourceTables[i] = pRenderDevice->CreateDeviceBuffer([&](DeviceBufferProxy& proxy)
		{
			proxy.SetSizeInBytes(resourceTablesMemorySizeInBytes[i]);
			proxy.SetStride(resourceTablesMemoryStrides[i]);
			proxy.InitialState = DeviceResource::State::CopyDest;
		});
		m_Allocators[i].Reset(resourceTablesMemorySizeInBytes[i]);
	}
}

void GpuScene::UploadLights(RenderContext& RenderContext)
{
	PIXMarker(RenderContext->GetD3DCommandList(), L"Upload Lights");

	auto pLightTable = SV_pRenderDevice->GetBuffer(m_ResourceTables[LightTable]);
	auto pUploadLightTable = SV_pRenderDevice->GetBuffer(m_UploadResourceTables[LightTable]);

	std::vector<HLSLPolygonalLight> hlslLights; hlslLights.reserve(pScene->Lights.size());
	for (auto& light : pScene->Lights)
	{
		light.GpuLightIndex = hlslLights.size();

		hlslLights.push_back(GetShaderLightDesc(light));
	}

	Upload(LightTable, hlslLights.data(), hlslLights.size() * sizeof(HLSLPolygonalLight), pUploadLightTable);
	RenderContext->CopyResource(pLightTable, pUploadLightTable);
	RenderContext->TransitionBarrier(pLightTable, DeviceResource::State::PixelShaderResource | DeviceResource::State::NonPixelShaderResource);
}

void GpuScene::UploadMaterials(RenderContext& RenderContext)
{
	PIXMarker(RenderContext->GetD3DCommandList(), L"Upload Materials");

	GpuTextureAllocator.Stage(*pScene, RenderContext);

	auto pMaterialTable = SV_pRenderDevice->GetBuffer(m_ResourceTables[MaterialTable]);
	auto pUploadMaterialTable = SV_pRenderDevice->GetBuffer(m_UploadResourceTables[MaterialTable]);

	std::vector<HLSLMaterial> hlslMaterials; hlslMaterials.reserve(pScene->Materials.size());
	for (auto& material : pScene->Materials)
	{
		material.GpuMaterialIndex = hlslMaterials.size();

		hlslMaterials.push_back(GetShaderMaterialDesc(material));
	}

	Upload(MaterialTable, hlslMaterials.data(), hlslMaterials.size() * sizeof(HLSLMaterial), pUploadMaterialTable);
	RenderContext->CopyResource(pMaterialTable, pUploadMaterialTable);
	RenderContext->TransitionBarrier(pMaterialTable, DeviceResource::State::PixelShaderResource | DeviceResource::State::NonPixelShaderResource);
}

void GpuScene::UploadModels(RenderContext& RenderContext)
{
	PIXMarker(RenderContext->GetD3DCommandList(), L"Upload Models");

	auto pVertexBuffer = SV_pRenderDevice->GetBuffer(m_ResourceTables[VertexBuffer]);
	auto pUploadVertexBuffer = SV_pRenderDevice->GetBuffer(m_UploadResourceTables[VertexBuffer]);

	auto pIndexBuffer = SV_pRenderDevice->GetBuffer(m_ResourceTables[IndexBuffer]);
	auto pUploadIndexBuffer = SV_pRenderDevice->GetBuffer(m_UploadResourceTables[IndexBuffer]);

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
			LOG_INFO("{} Loaded", model.Path);
		}

		// Add model's meshes into RTBLAS
		RTBLAS rtblas;
		for (auto& mesh : model.Meshes)
		{
			// Update mesh's BLAS Index
			mesh.BottomLevelAccelerationStructureIndex	= m_RaytracingBottomLevelAccelerationStructures.size();

			RaytracingGeometryDesc Desc					= {};
			Desc.pVertexBuffer							= pVertexBuffer;
			Desc.VertexStride							= sizeof(Vertex);
			Desc.pIndexBuffer							= pIndexBuffer;
			Desc.IndexStride							= sizeof(unsigned int);
			Desc.IsOpaque								= true;
			Desc.NumVertices							= mesh.VertexCount;
			Desc.VertexOffset							= mesh.BaseVertexLocation;
			Desc.NumIndices								= mesh.IndexCount;
			Desc.IndexOffset							= mesh.StartIndexLocation;

			rtblas.BLAS.AddGeometry(Desc);
		}
		m_RaytracingBottomLevelAccelerationStructures.push_back(rtblas);
	}

	RenderContext->CopyResource(pVertexBuffer, pUploadVertexBuffer);
	RenderContext->TransitionBarrier(pVertexBuffer, DeviceResource::State::VertexBuffer | DeviceResource::State::NonPixelShaderResource);

	RenderContext->CopyResource(pIndexBuffer, pUploadIndexBuffer);
	RenderContext->TransitionBarrier(pIndexBuffer, DeviceResource::State::IndexBuffer | DeviceResource::State::NonPixelShaderResource);
}

void GpuScene::UploadModelInstances(RenderContext& RenderContext)
{
	PIXMarker(RenderContext->GetD3DCommandList(), L"Upload Model Instances");

	auto pGeometryInfoTable = SV_pRenderDevice->GetBuffer(m_ResourceTables[GeometryInfoTable]);
	auto pUploadGeometryInfoTable = SV_pRenderDevice->GetBuffer(m_UploadResourceTables[GeometryInfoTable]);

	size_t instanceID = 0;
	std::vector<GeometryInfo> geometryInfos;
	for (auto& modelInstance : pScene->ModelInstances)
	{
		for (auto& meshInstance : modelInstance.MeshInstances)
		{
			meshInstance.InstanceID = instanceID++;

			GeometryInfo info		= {};
			info.VertexOffset		= meshInstance.pMesh->BaseVertexLocation;
			info.IndexOffset		= meshInstance.pMesh->StartIndexLocation;
			info.MaterialIndex		= modelInstance.pMaterial->GpuMaterialIndex;
			XMStoreFloat4x4(&info.World, XMMatrixTranspose(meshInstance.Transform.Matrix()));

			geometryInfos.push_back(info);
		}
	}

	Upload(GeometryInfoTable, geometryInfos.data(), geometryInfos.size() * sizeof(GeometryInfo), pUploadGeometryInfoTable);
	RenderContext->CopyResource(pGeometryInfoTable, pUploadGeometryInfoTable);
	RenderContext->TransitionBarrier(pGeometryInfoTable, DeviceResource::State::PixelShaderResource | DeviceResource::State::NonPixelShaderResource);

	CreateBottomLevelAS(RenderContext);
	CreateTopLevelAS(RenderContext);
}

void GpuScene::DisposeResources()
{
	SV_pRenderDevice->Destroy(&m_RaytracingTopLevelAccelerationStructure.Handles.Scratch);
	SV_pRenderDevice->Destroy(&m_RaytracingTopLevelAccelerationStructure.Handles.InstanceDescs);

	for (auto& rtblas : m_RaytracingBottomLevelAccelerationStructures)
	{
		SV_pRenderDevice->Destroy(&rtblas.Handles.Scratch);
	}

	GpuTextureAllocator.DisposeResources();
}

void GpuScene::RenderGui()
{
	if (ImGui::Begin("Scene"))
	{
		for (auto& light : pScene->Lights)
		{
			light.RenderGui();
		}

		for (auto& material : pScene->Materials)
		{
			material.RenderGui();
		}
	}
	ImGui::End();
}

void GpuScene::Update(float AspectRatio, RenderContext& RenderContext)
{
	PIXMarker(RenderContext->GetD3DCommandList(), L"Update");

	pScene->Camera.SetAspectRatio(AspectRatio);

	{
		auto pLightTable = SV_pRenderDevice->GetBuffer(m_ResourceTables[LightTable]);
		auto pUploadLightTable = SV_pRenderDevice->GetBuffer(m_UploadResourceTables[LightTable]);
		for (auto& light : pScene->Lights)
		{
			if (light.Dirty)
			{
				light.Dirty = false;

				HLSLPolygonalLight hlslPolygonalLight = GetShaderLightDesc(light);
				pUploadLightTable->Update<HLSLPolygonalLight>(light.GpuLightIndex, hlslPolygonalLight);
			}
		}
		RenderContext->CopyResource(pLightTable, pUploadLightTable);
		RenderContext->TransitionBarrier(pLightTable, DeviceResource::State::PixelShaderResource | DeviceResource::State::NonPixelShaderResource);
	}

	{
		auto pMaterialTable = SV_pRenderDevice->GetBuffer(m_ResourceTables[MaterialTable]);
		auto pUploadMaterialTable = SV_pRenderDevice->GetBuffer(m_UploadResourceTables[MaterialTable]);
		for (auto& material : pScene->Materials)
		{
			if (material.Dirty)
			{
				material.Dirty = false;

				HLSLMaterial hlslMaterial = GetShaderMaterialDesc(material);
				pUploadMaterialTable->Update<HLSLMaterial>(material.GpuMaterialIndex, hlslMaterial);
			}
		}
		RenderContext->CopyResource(pMaterialTable, pUploadMaterialTable);
		RenderContext->TransitionBarrier(pMaterialTable, DeviceResource::State::PixelShaderResource | DeviceResource::State::NonPixelShaderResource);
	}

	/*{
		auto pGeometryInfoTable = SV_pRenderDevice->GetBuffer(m_ResourceTables[GeometryInfoTable]);
		auto pUploadGeometryInfoTable = SV_pRenderDevice->GetBuffer(m_UploadResourceTables[GeometryInfoTable]);
		for (const auto& modelInstance : pScene->ModelInstances)
		{
			for (const auto& meshInstance : modelInstance.MeshInstances)
			{
				GeometryInfo hlslMesh = GetShaderMeshDesc(modelInstance.pMaterial->GpuMaterialIndex, meshInstance);

				pUploadGeometryInfoTable->Update<GeometryInfo>(meshInstance.InstanceID, hlslMesh);
			}
		}
		RenderContext->CopyResource(pGeometryInfoTable, pUploadGeometryInfoTable);
		RenderContext->TransitionBarrier(pGeometryInfoTable, DeviceResource::State::PixelShaderResource | DeviceResource::State::NonPixelShaderResource);
	}*/
	RenderContext->FlushResourceBarriers();
}

size_t GpuScene::Upload(EResource Type, const void* pData, size_t ByteSize, DeviceBuffer* pUploadBuffer)
{
	if (pData && ByteSize != 0)
	{
		auto pair = m_Allocators[Type].Allocate(ByteSize);
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
		rtblas.BLAS.ComputeMemoryRequirements(&SV_pRenderDevice->Device, false, &scratchSizeInBytes, &resultSizeInBytes);

		// BLAS Scratch
		rtblas.Handles.Scratch = SV_pRenderDevice->CreateDeviceBuffer([=](DeviceBufferProxy& proxy)
		{
			proxy.SetSizeInBytes(scratchSizeInBytes);
			proxy.BindFlags = DeviceResource::BindFlags::AccelerationStructure;
			proxy.InitialState = DeviceResource::State::UnorderedAccess;
		});

		// BLAS Result
		rtblas.Handles.Result = SV_pRenderDevice->CreateDeviceBuffer([=](DeviceBufferProxy& proxy)
		{
			proxy.SetSizeInBytes(resultSizeInBytes);
			proxy.BindFlags = DeviceResource::BindFlags::AccelerationStructure;
			proxy.InitialState = DeviceResource::State::AccelerationStructure;
		});

		DeviceBuffer* pResult = SV_pRenderDevice->GetBuffer(rtblas.Handles.Result);
		DeviceBuffer* pScratch = SV_pRenderDevice->GetBuffer(rtblas.Handles.Scratch);

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
			RTBLAS&					RTBLAS		= m_RaytracingBottomLevelAccelerationStructures[meshInstance.pMesh->BottomLevelAccelerationStructureIndex];
			DeviceBuffer*			pBLAS		= SV_pRenderDevice->GetBuffer(RTBLAS.Handles.Result);
			RaytracingInstanceDesc	Desc		= {};

			Desc.AccelerationStructure			= pBLAS->GetGpuVirtualAddress();
			Desc.Transform						= meshInstance.Transform.Matrix();
			Desc.InstanceID						= meshInstance.InstanceID;
			Desc.InstanceMask					= RAYTRACING_INSTANCEMASK_ALL;

			Desc.HitGroupIndex					= hitGroupIndex;

			m_RaytracingTopLevelAccelerationStructure.TLAS.AddInstance(Desc);

			hitGroupIndex++;
		}
	}

	UINT64 scratchSizeInBytes, resultSizeInBytes, instanceDescsSizeInBytes;
	m_RaytracingTopLevelAccelerationStructure.TLAS.ComputeMemoryRequirements(&SV_pRenderDevice->Device, true, &scratchSizeInBytes, &resultSizeInBytes, &instanceDescsSizeInBytes);

	// TLAS Scratch
	m_RaytracingTopLevelAccelerationStructure.Handles.Scratch = SV_pRenderDevice->CreateDeviceBuffer([=](DeviceBufferProxy& proxy)
	{
		proxy.SetSizeInBytes(scratchSizeInBytes);
		proxy.BindFlags = DeviceResource::BindFlags::AccelerationStructure;
		proxy.InitialState = DeviceResource::State::UnorderedAccess;
	});

	// TLAS Result
	m_RaytracingTopLevelAccelerationStructure.Handles.Result = SV_pRenderDevice->CreateDeviceBuffer([=](DeviceBufferProxy& proxy)
	{
		proxy.SetSizeInBytes(resultSizeInBytes);
		proxy.BindFlags = DeviceResource::BindFlags::AccelerationStructure;
		proxy.InitialState = DeviceResource::State::AccelerationStructure;
	});

	m_RaytracingTopLevelAccelerationStructure.Handles.InstanceDescs = SV_pRenderDevice->CreateDeviceBuffer([=](DeviceBufferProxy& proxy)
	{
		proxy.SetSizeInBytes(instanceDescsSizeInBytes);
		proxy.SetCpuAccess(DeviceBuffer::CpuAccess::Write);
	});

	DeviceBuffer* pScratch			= SV_pRenderDevice->GetBuffer(m_RaytracingTopLevelAccelerationStructure.Handles.Scratch);
	DeviceBuffer* pResult			= SV_pRenderDevice->GetBuffer(m_RaytracingTopLevelAccelerationStructure.Handles.Result);
	DeviceBuffer* pInstanceDescs	= SV_pRenderDevice->GetBuffer(m_RaytracingTopLevelAccelerationStructure.Handles.InstanceDescs);

	pResult->SetDebugName(L"RTTLAS Result");

	m_RaytracingTopLevelAccelerationStructure.TLAS.Generate(RenderContext.GetCommandContext(), pScratch, pResult, pInstanceDescs);
}