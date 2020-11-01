#include "pch.h"
#include "GpuScene.h"
#include "RendererRegistry.h"

#define RAYTRACING_INSTANCEMASK_ALL 	(0xff)
#define RAYTRACING_INSTANCEMASK_OPAQUE 	(1 << 0)
#define RAYTRACING_INSTANCEMASK_LIGHT	(1 << 1)

namespace
{
	static constexpr size_t NumLights					= 1000;
	static constexpr size_t NumMaterials				= 1000;

	static constexpr size_t LightBufferByteSize			= NumLights * sizeof(HLSL::PolygonalLight);
	static constexpr size_t MaterialBufferByteSize		= NumMaterials * sizeof(HLSL::Material);
	static constexpr size_t VertexBufferByteSize		= 30_MiB;
	static constexpr size_t IndexBufferByteSize			= 30_MiB;
	static constexpr size_t GeometryInfoBufferByteSize	= 30_MiB;
}

HLSL::PolygonalLight GetHLSLLightDesc(const PolygonalLight& Light)
{
	matrix World; XMStoreFloat4x4(&World, XMMatrixTranspose(Light.Transform.Matrix()));
	float4 Orientation; XMStoreFloat4(&Orientation, DirectX::XMVector3Normalize(Light.Transform.Forward()));
	return
	{
		.Position		= Light.Transform.Position,
		.Orientation	= Orientation,
		.World			= World,
		.Color			= Light.Color,
		.Luminance		= Light.GetLuminance(),
		.Width			= Light.GetWidth(),
		.Height			= Light.GetHeight()
	};
}

HLSL::Material GetHLSLMaterialDesc(const Material& Material)
{
	return
	{
		.Albedo				= Material.Albedo,
		.Emissive			= Material.Emissive,
		.Specular			= Material.Specular,
		.Refraction			= Material.Refraction,
		.SpecularChance		= Material.SpecularChance,
		.Roughness			= Material.Roughness,
		.Metallic			= Material.Metallic,
		.Fuzziness			= Material.Fuzziness,
		.IndexOfRefraction	= Material.IndexOfRefraction,
		.Model				= Material.Model,
		.TextureIndices		=
		{
			Material.TextureIndices[0],
			Material.TextureIndices[1],
			Material.TextureIndices[2],
			Material.TextureIndices[3],
			Material.TextureIndices[4]
		}
	};
}

HLSL::Camera GetHLSLCameraDesc(const PerspectiveCamera& Camera)
{
	DirectX::XMFLOAT4 Position = { Camera.Transform.Position.x, Camera.Transform.Position.y, Camera.Transform.Position.z, 1.0f };
	DirectX::XMFLOAT4 U, V, W;
	DirectX::XMFLOAT4X4 View, Projection, ViewProjection, InvView, InvProjection;
	XMStoreFloat4(&U, Camera.GetUVector());
	XMStoreFloat4(&V, Camera.GetVVector());
	XMStoreFloat4(&W, Camera.GetWVector());
	XMStoreFloat4x4(&View, XMMatrixTranspose(Camera.ViewMatrix()));
	XMStoreFloat4x4(&Projection, XMMatrixTranspose(Camera.ProjectionMatrix()));
	XMStoreFloat4x4(&ViewProjection, XMMatrixTranspose(Camera.ViewProjectionMatrix()));
	XMStoreFloat4x4(&InvView, XMMatrixTranspose(Camera.InverseViewMatrix()));
	XMStoreFloat4x4(&InvProjection, XMMatrixTranspose(Camera.InverseProjectionMatrix()));

	return
	{
		.NearZ				= Camera.NearZ(),
		.FarZ				= Camera.FarZ(),
		.ExposureValue100	= Camera.ExposureValue100(),
		._padding1			= 0.0f,

		.FocalLength		= Camera.FocalLength,
		.RelativeAperture	= Camera.RelativeAperture,
		.ShutterTime		= Camera.ShutterTime,
		.SensorSensitivity	= Camera.SensorSensitivity,

		.Position			= Position,
		.U					= U,
		.V					= V,
		.W					= W,

		.View				= View,
		.Projection			= Projection,
		.ViewProjection		= ViewProjection,
		.InvView			= InvView,
		.InvProjection		= InvProjection
	};
}

HLSL::Mesh GetShaderMeshDesc(size_t MaterialIndex, const MeshInstance& MeshInstance)
{
	matrix World; XMStoreFloat4x4(&World, XMMatrixTranspose(MeshInstance.Transform.Matrix()));
	return
	{
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
	size_t resourceTablesMemoryStrides[NumResources] = { sizeof(HLSL::PolygonalLight), sizeof(HLSL::Material), sizeof(Vertex), sizeof(unsigned int), sizeof(HLSL::Mesh) };

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

	std::vector<HLSL::PolygonalLight> hlslLights; hlslLights.reserve(pScene->Lights.size());
	for (auto& light : pScene->Lights)
	{
		light.SetGpuLightIndex(hlslLights.size());

		hlslLights.push_back(GetHLSLLightDesc(light));
	}

	Upload(LightTable, hlslLights.data(), hlslLights.size() * sizeof(HLSL::PolygonalLight), pUploadLightTable);
	RenderContext->CopyResource(pLightTable, pUploadLightTable);
	RenderContext->TransitionBarrier(pLightTable, DeviceResource::State::PixelShaderResource | DeviceResource::State::NonPixelShaderResource);
}

void GpuScene::UploadMaterials(RenderContext& RenderContext)
{
	PIXMarker(RenderContext->GetD3DCommandList(), L"Upload Materials");

	GpuTextureAllocator.Stage(*pScene, RenderContext);

	auto pMaterialTable = SV_pRenderDevice->GetBuffer(m_ResourceTables[MaterialTable]);
	auto pUploadMaterialTable = SV_pRenderDevice->GetBuffer(m_UploadResourceTables[MaterialTable]);

	std::vector<HLSL::Material> hlslMaterials; hlslMaterials.reserve(pScene->Materials.size());
	for (auto& material : pScene->Materials)
	{
		material.GpuMaterialIndex = hlslMaterials.size();

		hlslMaterials.push_back(GetHLSLMaterialDesc(material));
	}

	Upload(MaterialTable, hlslMaterials.data(), hlslMaterials.size() * sizeof(HLSL::Material), pUploadMaterialTable);
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

	auto pGeometryInfoTable = SV_pRenderDevice->GetBuffer(m_ResourceTables[MeshTable]);
	auto pUploadGeometryInfoTable = SV_pRenderDevice->GetBuffer(m_UploadResourceTables[MeshTable]);

	size_t instanceID = 0;
	std::vector<HLSL::Mesh> hlslMeshes;
	for (auto& modelInstance : pScene->ModelInstances)
	{
		for (auto& meshInstance : modelInstance.MeshInstances)
		{
			meshInstance.InstanceID = instanceID++;

			HLSL::Mesh info			= {};
			info.VertexOffset		= meshInstance.pMesh->BaseVertexLocation;
			info.IndexOffset		= meshInstance.pMesh->StartIndexLocation;
			info.MaterialIndex		= modelInstance.pMaterial->GpuMaterialIndex;
			XMStoreFloat4x4(&info.World, XMMatrixTranspose(meshInstance.Transform.Matrix()));

			hlslMeshes.push_back(info);
		}
	}

	Upload(MeshTable, hlslMeshes.data(), hlslMeshes.size() * sizeof(HLSL::Mesh), pUploadGeometryInfoTable);
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
			if (light.IsDirty())
			{
				light.ResetDirtyFlag();

				HLSL::PolygonalLight hlslPolygonalLight = GetHLSLLightDesc(light);
				pUploadLightTable->Update<HLSL::PolygonalLight>(light.GetGpuLightIndex(), hlslPolygonalLight);
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

				HLSL::Material hlslMaterial = GetHLSLMaterialDesc(material);
				pUploadMaterialTable->Update<HLSL::Material>(material.GpuMaterialIndex, hlslMaterial);
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

HLSL::Camera GpuScene::GetHLSLCamera() const
{
	return GetHLSLCameraDesc(pScene->Camera);
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
	PIXMarker(RenderContext->GetD3DCommandList(), L"Bottom Level Acceleration Structure Generation");

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
	PIXMarker(RenderContext->GetD3DCommandList(), L"Top Level Acceleration Structure Generation");

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

			Desc.InstanceContributionToHitGroupIndex					= hitGroupIndex;

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

	pResult->SetDebugName(L"Ray Tracing Top Level Acceleration Structure");

	m_RaytracingTopLevelAccelerationStructure.TLAS.Generate(RenderContext.GetCommandContext(), pScratch, pResult, pInstanceDescs);
}