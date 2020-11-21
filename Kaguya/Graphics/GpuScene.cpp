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
	static constexpr size_t MeshBufferByteSize			= 30_MiB;
	static constexpr size_t VertexBufferByteSize		= 30_MiB;
	static constexpr size_t IndexBufferByteSize			= 30_MiB;
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
	DirectX::XMFLOAT4X4 View, Projection, ViewProjection, InvView, InvProjection, InvViewProjection;
	
	XMStoreFloat4(&U, Camera.GetUVector());
	XMStoreFloat4(&V, Camera.GetVVector());
	XMStoreFloat4(&W, Camera.GetWVector());

	XMStoreFloat4x4(&View, XMMatrixTranspose(Camera.ViewMatrix()));
	XMStoreFloat4x4(&Projection, XMMatrixTranspose(Camera.ProjectionMatrix()));
	XMStoreFloat4x4(&ViewProjection, XMMatrixTranspose(Camera.ViewProjectionMatrix()));
	XMStoreFloat4x4(&InvView, XMMatrixTranspose(Camera.InverseViewMatrix()));
	XMStoreFloat4x4(&InvProjection, XMMatrixTranspose(Camera.InverseProjectionMatrix()));
	XMStoreFloat4x4(&InvViewProjection, XMMatrixTranspose(Camera.InverseViewProjectionMatrix()));

	return
	{
		.NearZ						= Camera.NearZ(),
		.FarZ						= Camera.FarZ(),
		.ExposureValue100			= Camera.ExposureValue100(),
		._padding1					= 0.0f,

		.FocalLength				= Camera.FocalLength,
		.RelativeAperture			= Camera.RelativeAperture,
		.ShutterTime				= Camera.ShutterTime,
		.SensorSensitivity			= Camera.SensorSensitivity,

		.Position					= Position,
		.U							= U,
		.V							= V,
		.W							= W,

		.View						= View,
		.Projection					= Projection,
		.ViewProjection				= ViewProjection,
		.InvView					= InvView,
		.InvProjection				= InvProjection,
		.InvViewProjection			= InvViewProjection
	};
}

HLSL::Mesh GetShaderMeshDesc(size_t MaterialIndex, const MeshInstance& MeshInstance)
{
	matrix World;			XMStoreFloat4x4(&World, XMMatrixTranspose(MeshInstance.Transform.Matrix()));
	matrix PreviousWorld;	XMStoreFloat4x4(&PreviousWorld, XMMatrixTranspose(MeshInstance.PreviousTransform.Matrix()));
	return
	{
		.VertexOffset	= MeshInstance.pMesh->BaseVertexLocation,
		.IndexOffset	= MeshInstance.pMesh->StartIndexLocation,
		.MaterialIndex	= (uint)MaterialIndex,
		.World			= World,
		.PreviousWorld	= PreviousWorld
	};
}

GpuScene::GpuScene(RenderDevice* pRenderDevice)
	: SV_pRenderDevice(pRenderDevice),
	pScene(nullptr),
	GpuTextureAllocator(pRenderDevice)
{
	m_LightTable = pRenderDevice->CreateDeviceBuffer([&](DeviceBufferProxy& proxy)
	{
		proxy.SetSizeInBytes(LightBufferByteSize);
		proxy.SetStride(sizeof(HLSL::PolygonalLight));
		proxy.SetCpuAccess(DeviceBuffer::CpuAccess::Write);
	});

	m_MaterialTable = pRenderDevice->CreateDeviceBuffer([&](DeviceBufferProxy& proxy)
	{
		proxy.SetSizeInBytes(MaterialBufferByteSize);
		proxy.SetStride(sizeof(HLSL::Material));
		proxy.SetCpuAccess(DeviceBuffer::CpuAccess::Write);
	});

	m_MeshTable = pRenderDevice->CreateDeviceBuffer([&](DeviceBufferProxy& proxy)
	{
		proxy.SetSizeInBytes(MeshBufferByteSize);
		proxy.SetStride(sizeof(HLSL::Mesh));
		proxy.SetCpuAccess(DeviceBuffer::CpuAccess::Write);
	});
	
	// Vertex Buffer
	m_VertexBuffer = pRenderDevice->CreateDeviceBuffer([&](DeviceBufferProxy& proxy)
	{
		proxy.SetSizeInBytes(VertexBufferByteSize);
		proxy.SetStride(sizeof(Vertex));
		proxy.InitialState = DeviceResource::State::NonPixelShaderResource;
	});

	m_UploadVertexBuffer = pRenderDevice->CreateDeviceBuffer([&](DeviceBufferProxy& proxy)
	{
		proxy.SetSizeInBytes(VertexBufferByteSize);
		proxy.SetStride(sizeof(Vertex));
		proxy.SetCpuAccess(DeviceBuffer::CpuAccess::Write);
	});

	m_VertexBufferAllocator.Reset(VertexBufferByteSize);

	// Index Buffer
	m_IndexBuffer = pRenderDevice->CreateDeviceBuffer([&](DeviceBufferProxy& proxy)
	{
		proxy.SetSizeInBytes(IndexBufferByteSize);
		proxy.SetStride(sizeof(unsigned int));
		proxy.InitialState = DeviceResource::State::NonPixelShaderResource;
	});

	m_UploadIndexBuffer = pRenderDevice->CreateDeviceBuffer([&](DeviceBufferProxy& proxy)
	{
		proxy.SetSizeInBytes(IndexBufferByteSize);
		proxy.SetStride(sizeof(unsigned int));
		proxy.SetCpuAccess(DeviceBuffer::CpuAccess::Write);
	});

	m_IndexBufferAllocator.Reset(IndexBufferByteSize);
}

void GpuScene::UploadTextures(RenderContext& RenderContext)
{
	GpuTextureAllocator.Stage(*pScene, RenderContext);
}

void GpuScene::UploadModels(RenderContext& RenderContext)
{
	PIXMarker(RenderContext->GetD3DCommandList(), L"Upload Models");

	auto pVertexBuffer = SV_pRenderDevice->GetBuffer(m_VertexBuffer);
	auto pUploadVertexBuffer = SV_pRenderDevice->GetBuffer(m_UploadVertexBuffer);

	auto pIndexBuffer = SV_pRenderDevice->GetBuffer(m_IndexBuffer);
	auto pUploadIndexBuffer = SV_pRenderDevice->GetBuffer(m_UploadIndexBuffer);

	for (auto& model : pScene->Models)
	{
		UINT64 totalVertexBytes = model.Vertices.size() * sizeof(Vertex);
		UINT64 totalIndexBytes = model.Indices.size() * sizeof(UINT);

		size_t vertexByteOffset, indexByteOffset;

		// Stage vertex
		{
			auto pair = m_VertexBufferAllocator.Allocate(totalVertexBytes);
			assert(pair.has_value() && "Unable to allocate data, consider increasing memory");
			auto [offset, size] = pair.value();

			auto pGPU = pUploadVertexBuffer->Map();
			auto pCPU = model.Vertices.data();
			memcpy(&pGPU[offset], pCPU, size);
			vertexByteOffset = offset;
		}

		{
			auto pair = m_IndexBufferAllocator.Allocate(totalIndexBytes);
			assert(pair.has_value() && "Unable to allocate data, consider increasing memory");
			auto [offset, size] = pair.value();

			auto pGPU = pUploadIndexBuffer->Map();
			auto pCPU = model.Indices.data();
			memcpy(&pGPU[offset], pCPU, size);
			indexByteOffset = offset;
		}

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
			mesh.BottomLevelAccelerationStructureIndex = m_RaytracingBottomLevelAccelerationStructures.size();

			RaytracingGeometryDesc Desc = {};
			Desc.pVertexBuffer = pVertexBuffer;
			Desc.VertexStride = sizeof(Vertex);
			Desc.pIndexBuffer = pIndexBuffer;
			Desc.IndexStride = sizeof(unsigned int);
			Desc.IsOpaque = true;
			Desc.NumVertices = mesh.VertexCount;
			Desc.VertexOffset = mesh.BaseVertexLocation;
			Desc.NumIndices = mesh.IndexCount;
			Desc.IndexOffset = mesh.StartIndexLocation;

			rtblas.BLAS.AddGeometry(Desc);
		}
		m_RaytracingBottomLevelAccelerationStructures.push_back(rtblas);
	}

	RenderContext->CopyResource(pVertexBuffer, pUploadVertexBuffer);
	RenderContext->TransitionBarrier(pVertexBuffer, DeviceResource::State::VertexBuffer | DeviceResource::State::NonPixelShaderResource);

	RenderContext->CopyResource(pIndexBuffer, pUploadIndexBuffer);
	RenderContext->TransitionBarrier(pIndexBuffer, DeviceResource::State::IndexBuffer | DeviceResource::State::NonPixelShaderResource);

	RenderContext->FlushResourceBarriers();

	CreateBottomLevelAS(RenderContext);
}

void GpuScene::DisposeResources()
{
	for (auto& rtblas : m_RaytracingBottomLevelAccelerationStructures)
	{
		SV_pRenderDevice->Destroy(&rtblas.Handles.Scratch);
	}

	GpuTextureAllocator.DisposeResources();
}

void GpuScene::UploadLights()
{
	auto pLightTable = SV_pRenderDevice->GetBuffer(m_LightTable);

	std::vector<HLSL::PolygonalLight> hlslLights; hlslLights.reserve(pScene->Lights.size());
	for (auto& light : pScene->Lights)
	{
		light.SetGpuLightIndex(hlslLights.size());

		hlslLights.push_back(GetHLSLLightDesc(light));
	}

	auto pGPU = pLightTable->Map();
	auto pCPU = hlslLights.data();
	memcpy(pGPU, pCPU, hlslLights.size() * sizeof(HLSL::PolygonalLight));
}

void GpuScene::UploadMaterials()
{
	auto pMaterialTable = SV_pRenderDevice->GetBuffer(m_MaterialTable);

	std::vector<HLSL::Material> hlslMaterials; hlslMaterials.reserve(pScene->Materials.size());
	for (auto& material : pScene->Materials)
	{
		material.GpuMaterialIndex = hlslMaterials.size();

		hlslMaterials.push_back(GetHLSLMaterialDesc(material));
	}

	auto pGPU = pMaterialTable->Map();
	auto pCPU = hlslMaterials.data();
	memcpy(pGPU, pCPU, hlslMaterials.size() * sizeof(HLSL::Material));
}

void GpuScene::UploadMeshes()
{
	auto pMeshTable = SV_pRenderDevice->GetBuffer(m_MeshTable);

	std::vector<HLSL::Mesh> hlslMeshes;
	for (auto& modelInstance : pScene->ModelInstances)
	{
		for (auto& meshInstance : modelInstance.MeshInstances)
		{
			meshInstance.InstanceID = hlslMeshes.size();

			HLSL::Mesh hlslMesh		= {};
			hlslMesh.VertexOffset	= meshInstance.pMesh->BaseVertexLocation;
			hlslMesh.IndexOffset	= meshInstance.pMesh->StartIndexLocation;
			hlslMesh.MaterialIndex	= modelInstance.pMaterial->GpuMaterialIndex;
			XMStoreFloat4x4(&hlslMesh.World, XMMatrixTranspose(meshInstance.Transform.Matrix()));

			hlslMeshes.push_back(hlslMesh);
		}
	}

	auto pGPU = pMeshTable->Map();
	auto pCPU = hlslMeshes.data();
	memcpy(pGPU, pCPU, hlslMeshes.size() * sizeof(HLSL::Mesh));
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

bool GpuScene::Update(float AspectRatio, RenderContext& RenderContext)
{
	PIXMarker(RenderContext->GetD3DCommandList(), L"Update");

	pScene->Camera.SetAspectRatio(AspectRatio);

	CreateTopLevelAS(RenderContext);

	bool Refresh = false;

	auto pLightTable = SV_pRenderDevice->GetBuffer(m_LightTable);
	for (auto& light : pScene->Lights)
	{
		if (light.IsDirty())
		{
			Refresh |= true;

			light.ResetDirtyFlag();

			HLSL::PolygonalLight hlslPolygonalLight = GetHLSLLightDesc(light);
			pLightTable->Update<HLSL::PolygonalLight>(light.GetGpuLightIndex(), hlslPolygonalLight);
		}
	}

	auto pMaterialTable = SV_pRenderDevice->GetBuffer(m_MaterialTable);
	for (auto& material : pScene->Materials)
	{
		if (material.Dirty)
		{
			Refresh |= true;

			material.Dirty = false;

			HLSL::Material hlslMaterial = GetHLSLMaterialDesc(material);
			pMaterialTable->Update<HLSL::Material>(material.GpuMaterialIndex, hlslMaterial);
		}
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
	return Refresh;
}

HLSL::Camera GpuScene::GetHLSLCamera() const
{
	return GetHLSLCameraDesc(pScene->Camera);
}

HLSL::Camera GpuScene::GetHLSLPreviousCamera() const
{
	return GetHLSLCameraDesc(pScene->PreviousCamera);
}

void GpuScene::CreateBottomLevelAS(RenderContext& RenderContext)
{
	PIXMarker(RenderContext->GetD3DCommandList(), L"Bottom Level Acceleration Structure Generation");

	for (auto& rtblas : m_RaytracingBottomLevelAccelerationStructures)
	{
		UINT64 scratchSizeInBytes, resultSizeInBytes;
		rtblas.BLAS.ComputeMemoryRequirements(&SV_pRenderDevice->Device, &scratchSizeInBytes, &resultSizeInBytes);

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

	TopLevelAccelerationStructure TopLevelAccelerationStructure;

	size_t hitGroupIndex = 0;
	for (const auto& modelInstance : pScene->ModelInstances)
	{
		for (const auto& meshInstance : modelInstance.MeshInstances)
		{
			RTBLAS&					RTBLAS				= m_RaytracingBottomLevelAccelerationStructures[meshInstance.pMesh->BottomLevelAccelerationStructureIndex];
			DeviceBuffer*			pBLAS				= SV_pRenderDevice->GetBuffer(RTBLAS.Handles.Result);

			RaytracingInstanceDesc	Desc				= {};
			Desc.AccelerationStructure					= pBLAS->GetGpuVirtualAddress();
			Desc.Transform								= meshInstance.Transform.Matrix();
			Desc.InstanceID								= meshInstance.InstanceID;
			Desc.InstanceMask							= RAYTRACING_INSTANCEMASK_ALL;

			Desc.InstanceContributionToHitGroupIndex	= hitGroupIndex;

			TopLevelAccelerationStructure.AddInstance(Desc);

			hitGroupIndex++;
		}
	}

	UINT64 scratchSizeInBytes, resultSizeInBytes, instanceDescsSizeInBytes;
	TopLevelAccelerationStructure.ComputeMemoryRequirements(&SV_pRenderDevice->Device, &scratchSizeInBytes, &resultSizeInBytes, &instanceDescsSizeInBytes);

	DeviceBuffer* pScratch			= SV_pRenderDevice->GetBuffer(m_TopLevelAccelerationStructure.Scratch);
	DeviceBuffer* pResult			= SV_pRenderDevice->GetBuffer(m_TopLevelAccelerationStructure.Result);
	DeviceBuffer* pInstanceDescs	= SV_pRenderDevice->GetBuffer(m_TopLevelAccelerationStructure.InstanceDescs);

	if (!pScratch || pScratch->GetSizeInBytes() < scratchSizeInBytes)
	{
		SV_pRenderDevice->Destroy(&m_TopLevelAccelerationStructure.Scratch);

		// TLAS Scratch
		m_TopLevelAccelerationStructure.Scratch = SV_pRenderDevice->CreateDeviceBuffer([=](DeviceBufferProxy& proxy)
		{
			proxy.SetSizeInBytes(scratchSizeInBytes);
			proxy.BindFlags = DeviceResource::BindFlags::AccelerationStructure;
			proxy.InitialState = DeviceResource::State::UnorderedAccess;
		});
		pScratch = SV_pRenderDevice->GetBuffer(m_TopLevelAccelerationStructure.Scratch);
	}

	if (!pResult || pResult->GetSizeInBytes() < resultSizeInBytes)
	{
		SV_pRenderDevice->Destroy(&m_TopLevelAccelerationStructure.Result);

		// TLAS Result
		m_TopLevelAccelerationStructure.Result = SV_pRenderDevice->CreateDeviceBuffer([=](DeviceBufferProxy& proxy)
		{
			proxy.SetSizeInBytes(resultSizeInBytes);
			proxy.BindFlags = DeviceResource::BindFlags::AccelerationStructure;
			proxy.InitialState = DeviceResource::State::AccelerationStructure;
		});
		pResult = SV_pRenderDevice->GetBuffer(m_TopLevelAccelerationStructure.Result);
	}
	
	if (!pInstanceDescs || pInstanceDescs->GetSizeInBytes() < instanceDescsSizeInBytes)
	{
		SV_pRenderDevice->Destroy(&m_TopLevelAccelerationStructure.InstanceDescs);

		// TLAS Instance Desc
		m_TopLevelAccelerationStructure.InstanceDescs = SV_pRenderDevice->CreateDeviceBuffer([=](DeviceBufferProxy& proxy)
		{
			proxy.SetSizeInBytes(instanceDescsSizeInBytes);
			proxy.SetCpuAccess(DeviceBuffer::CpuAccess::Write);
		});
		pInstanceDescs = SV_pRenderDevice->GetBuffer(m_TopLevelAccelerationStructure.InstanceDescs);
	}

	pResult->SetDebugName(L"Top Level Acceleration Structure");

	TopLevelAccelerationStructure.Generate(RenderContext.GetCommandContext(), pScratch, pResult, pInstanceDescs);
}