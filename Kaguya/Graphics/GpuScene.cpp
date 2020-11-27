#include "pch.h"
#include "GpuScene.h"

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
		.Albedo					= Material.Albedo,
		.Emissive				= Material.Emissive,
		.Specular				= Material.Specular,
		.Refraction				= Material.Refraction,
		.SpecularChance			= Material.SpecularChance,
		.Roughness				= Material.Roughness,
		.Metallic				= Material.Metallic,
		.Fuzziness				= Material.Fuzziness,
		.IndexOfRefraction		= Material.IndexOfRefraction,
		.Model					= Material.Model,
		.UseAttributeAsValues	= Material.UseAttributeAsValues, // If this is true, then the attributes above will be used rather than actual textures
		.TextureIndices			=
		{
			Material.TextureIndices[0],
			Material.TextureIndices[1],
			Material.TextureIndices[2],
			Material.TextureIndices[3],
			Material.TextureIndices[4]
		},
		.TextureChannel			=
		{
			Material.TextureChannel[0],
			Material.TextureChannel[1],
			Material.TextureChannel[2],
			Material.TextureChannel[3],
			Material.TextureChannel[4],
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

HLSL::Mesh GetHLSLMeshDesc(size_t MaterialIndex, const MeshInstance& MeshInstance)
{
	matrix World;			XMStoreFloat4x4(&World, XMMatrixTranspose(MeshInstance.Transform.Matrix()));
	matrix PreviousWorld;	XMStoreFloat4x4(&PreviousWorld, XMMatrixTranspose(MeshInstance.PreviousTransform.Matrix()));
	return
	{
		.VertexOffset				= MeshInstance.pMesh->BaseVertexLocation,
		.IndexOffset				= MeshInstance.pMesh->StartIndexLocation,
		.MeshletOffset				= MeshInstance.pMesh->MeshletOffset,
		.UniqueVertexIndexOffset	= MeshInstance.pMesh->UniqueVertexIndexOffset,
		.PrimitiveIndexOffset		= MeshInstance.pMesh->PrimitiveIndexOffset,
		.MaterialIndex				= (uint32_t)MaterialIndex,
		.World						= World,
		.PreviousWorld				= PreviousWorld
	};
}

GpuScene::GpuScene(RenderDevice* pRenderDevice)
	: pRenderDevice(pRenderDevice),
	pScene(nullptr),
	GpuTextureAllocator(pRenderDevice)
{
	m_LightTable = pRenderDevice->InitializeRenderResourceHandle(RenderResourceType::Buffer, "Light Table");
	m_MaterialTable = pRenderDevice->InitializeRenderResourceHandle(RenderResourceType::Buffer, "Material Table");
	m_MeshTable = pRenderDevice->InitializeRenderResourceHandle(RenderResourceType::Buffer, "Mesh Table");

	m_VertexBuffer = pRenderDevice->InitializeRenderResourceHandle(RenderResourceType::Buffer, "Vertex Buffer");
	m_UploadVertexBuffer = pRenderDevice->InitializeRenderResourceHandle(RenderResourceType::Buffer, "Vertex Buffer (Upload)");

	m_IndexBuffer = pRenderDevice->InitializeRenderResourceHandle(RenderResourceType::Buffer, "Index Buffer");
	m_UploadIndexBuffer = pRenderDevice->InitializeRenderResourceHandle(RenderResourceType::Buffer, "Index Buffer (Upload)");

	m_RaytracingTopLevelAccelerationStructure =
	{
		pRenderDevice->InitializeRenderResourceHandle(RenderResourceType::Buffer, "Top-Level Acceleration Structure (Scratch)"),
		pRenderDevice->InitializeRenderResourceHandle(RenderResourceType::Buffer, "Top-Level Acceleration Structure (Result)"),
		pRenderDevice->InitializeRenderResourceHandle(RenderResourceType::Buffer, "Top-Level Acceleration Structure (InstanceDescs)")
	};

	pRenderDevice->CreateBuffer(m_LightTable, [&](BufferProxy& proxy)
	{
		proxy.SetSizeInBytes(LightBufferByteSize);
		proxy.SetStride(sizeof(HLSL::PolygonalLight));
		proxy.SetCpuAccess(Buffer::CpuAccess::Write);
	});

	pRenderDevice->CreateBuffer(m_MaterialTable, [&](BufferProxy& proxy)
	{
		proxy.SetSizeInBytes(MaterialBufferByteSize);
		proxy.SetStride(sizeof(HLSL::Material));
		proxy.SetCpuAccess(Buffer::CpuAccess::Write);
	});

	pRenderDevice->CreateBuffer(m_MeshTable, [&](BufferProxy& proxy)
	{
		proxy.SetSizeInBytes(MeshBufferByteSize);
		proxy.SetStride(sizeof(HLSL::Mesh));
		proxy.SetCpuAccess(Buffer::CpuAccess::Write);
	});
	
	// Vertex Buffer
	pRenderDevice->CreateBuffer(m_VertexBuffer, [&](BufferProxy& proxy)
	{
		proxy.SetSizeInBytes(VertexBufferByteSize);
		proxy.SetStride(sizeof(Vertex));
		proxy.InitialState = Resource::State::NonPixelShaderResource;
	});

	pRenderDevice->CreateBuffer(m_UploadVertexBuffer, [&](BufferProxy& proxy)
	{
		proxy.SetSizeInBytes(VertexBufferByteSize);
		proxy.SetStride(sizeof(Vertex));
		proxy.SetCpuAccess(Buffer::CpuAccess::Write);
	});

	m_VertexBufferAllocator.Reset(VertexBufferByteSize);

	// Index Buffer
	pRenderDevice->CreateBuffer(m_IndexBuffer, [&](BufferProxy& proxy)
	{
		proxy.SetSizeInBytes(IndexBufferByteSize);
		proxy.SetStride(sizeof(unsigned int));
		proxy.InitialState = Resource::State::NonPixelShaderResource;
	});

	pRenderDevice->CreateBuffer(m_UploadIndexBuffer, [&](BufferProxy& proxy)
	{
		proxy.SetSizeInBytes(IndexBufferByteSize);
		proxy.SetStride(sizeof(unsigned int));
		proxy.SetCpuAccess(Buffer::CpuAccess::Write);
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

	auto pVertexBuffer = pRenderDevice->GetBuffer(m_VertexBuffer);
	auto pUploadVertexBuffer = pRenderDevice->GetBuffer(m_UploadVertexBuffer);

	auto pIndexBuffer = pRenderDevice->GetBuffer(m_IndexBuffer);
	auto pUploadIndexBuffer = pRenderDevice->GetBuffer(m_UploadIndexBuffer);

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

		// Stage index
		{
			auto pair = m_IndexBufferAllocator.Allocate(totalIndexBytes);
			assert(pair.has_value() && "Unable to allocate data, consider increasing memory");
			auto [offset, size] = pair.value();

			auto pGPU = pUploadIndexBuffer->Map();
			auto pCPU = model.Indices.data();
			memcpy(&pGPU[offset], pCPU, size);
			indexByteOffset = offset;
		}

		// Recalculate vertex and index offsets because all vertices and indices are in a single gpu buffer
		for (auto& mesh : model)
		{
			assert(vertexByteOffset % sizeof(Vertex) == 0 && "Vertex offset mismatch");
			assert(indexByteOffset % sizeof(UINT) == 0 && "Index offset mismatch");
			mesh.BaseVertexLocation += (vertexByteOffset / sizeof(Vertex));
			mesh.StartIndexLocation += (indexByteOffset / sizeof(UINT));
		}

		size_t NumMeshlets = 0;
		size_t NumUniqueVertexIndices = 0;
		size_t NumPrimitiveIndices = 0;
		for (auto& mesh : model)
		{
			NumMeshlets += mesh.Meshlets.size();
			NumUniqueVertexIndices += mesh.UniqueVertexIndices.size();
			NumPrimitiveIndices += mesh.PrimitiveIndices.size();
		}

		model.MeshletResource = pRenderDevice->InitializeRenderResourceHandle(RenderResourceType::Buffer, model.Path + " [Meshlet Resource]");
		model.UploadMeshletResource = pRenderDevice->InitializeRenderResourceHandle(RenderResourceType::Buffer, model.Path + " [Meshlet Resource] (Upload)");
		model.UniqueVertexIndexResource = pRenderDevice->InitializeRenderResourceHandle(RenderResourceType::Buffer, model.Path + " [Unique Vertex Index Resource]");
		model.UploadUniqueVertexIndexResource = pRenderDevice->InitializeRenderResourceHandle(RenderResourceType::Buffer, model.Path + " [Unique Vertex Index Resource] (Upload)");
		model.PrimitiveIndexResource = pRenderDevice->InitializeRenderResourceHandle(RenderResourceType::Buffer, model.Path + " [Primitive Index Resource]");
		model.UploadPrimitiveIndexResource = pRenderDevice->InitializeRenderResourceHandle(RenderResourceType::Buffer, model.Path + " [Primitive Index Resource] (Upload)");

		// Meshlet resource
		pRenderDevice->CreateBuffer(model.MeshletResource, [=](BufferProxy& proxy)
		{
			proxy.SetSizeInBytes(NumMeshlets * sizeof(Meshlet));
			proxy.SetStride(sizeof(Meshlet));
			proxy.InitialState = Resource::State::CopyDest;
		});

		pRenderDevice->CreateBuffer(model.UploadMeshletResource, [=](BufferProxy& proxy)
		{
			proxy.SetSizeInBytes(NumMeshlets * sizeof(Meshlet));
			proxy.SetStride(sizeof(Meshlet));
			proxy.SetCpuAccess(Buffer::CpuAccess::Write);
		});

		// Unique vertex index resource
		pRenderDevice->CreateBuffer(model.UniqueVertexIndexResource, [=](BufferProxy& proxy)
		{
			proxy.SetSizeInBytes(NumUniqueVertexIndices * sizeof(uint32_t));
			proxy.SetStride(sizeof(uint32_t));
			proxy.InitialState = Resource::State::CopyDest;
		});

		pRenderDevice->CreateBuffer(model.UploadUniqueVertexIndexResource, [=](BufferProxy& proxy)
		{
			proxy.SetSizeInBytes(NumUniqueVertexIndices * sizeof(uint32_t));
			proxy.SetStride(sizeof(uint32_t));
			proxy.SetCpuAccess(Buffer::CpuAccess::Write);
		});

		// Primitive index resource
		pRenderDevice->CreateBuffer(model.PrimitiveIndexResource, [=](BufferProxy& proxy)
		{
			proxy.SetSizeInBytes(NumPrimitiveIndices * sizeof(MeshletPrimitive));
			proxy.SetStride(sizeof(MeshletPrimitive));
			proxy.InitialState = Resource::State::CopyDest;
		});

		pRenderDevice->CreateBuffer(model.UploadPrimitiveIndexResource, [=](BufferProxy& proxy)
		{
			proxy.SetSizeInBytes(NumPrimitiveIndices * sizeof(MeshletPrimitive));
			proxy.SetStride(sizeof(MeshletPrimitive));
			proxy.SetCpuAccess(Buffer::CpuAccess::Write);
		});

		size_t MeshletOffset = 0;
		size_t UniqueVertexIndexOffset = 0;
		size_t PrimitiveIndexOffset = 0;
		for (auto& mesh : model)
		{
			mesh.MeshletOffset = MeshletOffset;
			mesh.UniqueVertexIndexOffset = UniqueVertexIndexOffset;
			mesh.PrimitiveIndexOffset = PrimitiveIndexOffset;

			// Stage meshlets
			{
				auto pResource = pRenderDevice->GetBuffer(model.MeshletResource);
				auto pUploadResource = pRenderDevice->GetBuffer(model.UploadMeshletResource);

				auto pGPU = pUploadResource->Map();
				auto pCPU = mesh.Meshlets.data();
				memcpy(&pGPU[MeshletOffset], pCPU, mesh.Meshlets.size() * sizeof(Meshlet));

				MeshletOffset += mesh.Meshlets.size();
			}

			// Stage unique vertex indices;
			{
				auto pResource = pRenderDevice->GetBuffer(model.UniqueVertexIndexResource);
				auto pUploadResource = pRenderDevice->GetBuffer(model.UploadUniqueVertexIndexResource);

				auto pGPU = pUploadResource->Map();
				auto pCPU = mesh.UniqueVertexIndices.data();
				memcpy(&pGPU[UniqueVertexIndexOffset], pCPU, mesh.UniqueVertexIndices.size() * sizeof(uint32_t));

				UniqueVertexIndexOffset += mesh.UniqueVertexIndices.size();
			}

			// Stage unique vertex indices;
			{
				auto pResource = pRenderDevice->GetBuffer(model.PrimitiveIndexResource);
				auto pUploadResource = pRenderDevice->GetBuffer(model.UploadPrimitiveIndexResource);

				auto pGPU = pUploadResource->Map();
				auto pCPU = mesh.PrimitiveIndices.data();
				memcpy(&pGPU[PrimitiveIndexOffset], pCPU, mesh.PrimitiveIndices.size() * sizeof(MeshletPrimitive));

				PrimitiveIndexOffset += mesh.PrimitiveIndices.size();
			}
		}

		RenderContext.CopyResource(model.MeshletResource, model.UploadMeshletResource);
		RenderContext.CopyResource(model.UniqueVertexIndexResource, model.UploadUniqueVertexIndexResource);
		RenderContext.CopyResource(model.PrimitiveIndexResource, model.UploadPrimitiveIndexResource);

		RenderContext.TransitionBarrier(model.MeshletResource, Resource::State::NonPixelShaderResource);
		RenderContext.TransitionBarrier(model.UniqueVertexIndexResource, Resource::State::NonPixelShaderResource);
		RenderContext.TransitionBarrier(model.PrimitiveIndexResource, Resource::State::NonPixelShaderResource);

		LOG_INFO("{} Loaded", model.Path);

		// Add model's meshes into RTBLAS
		RTBLAS rtblas;
		rtblas.Scratch	= pRenderDevice->InitializeRenderResourceHandle(RenderResourceType::Buffer, model.Path + " (Scratch)");
		rtblas.Result	= pRenderDevice->InitializeRenderResourceHandle(RenderResourceType::Buffer, model.Path + " (Result)");
		for (auto& mesh : model)
		{
			// Update mesh's BLAS Index
			mesh.BottomLevelAccelerationStructureIndex = m_RaytracingBottomLevelAccelerationStructures.size();

			RaytracingGeometryDesc Desc = {};
			Desc.pVertexBuffer			= pVertexBuffer;
			Desc.VertexStride			= sizeof(Vertex);
			Desc.pIndexBuffer			= pIndexBuffer;
			Desc.IndexStride			= sizeof(unsigned int);
			Desc.IsOpaque				= true;
			Desc.NumVertices			= mesh.VertexCount;
			Desc.VertexOffset			= mesh.BaseVertexLocation;
			Desc.NumIndices				= mesh.IndexCount;
			Desc.IndexOffset			= mesh.StartIndexLocation;

			rtblas.BLAS.AddGeometry(Desc);
		}
		m_RaytracingBottomLevelAccelerationStructures.push_back(rtblas);
	}

	RenderContext->CopyResource(pVertexBuffer, pUploadVertexBuffer);
	RenderContext->TransitionBarrier(pVertexBuffer, Resource::State::VertexBuffer | Resource::State::NonPixelShaderResource);

	RenderContext->CopyResource(pIndexBuffer, pUploadIndexBuffer);
	RenderContext->TransitionBarrier(pIndexBuffer, Resource::State::IndexBuffer | Resource::State::NonPixelShaderResource);

	RenderContext->FlushResourceBarriers();

	CreateBottomLevelAS(RenderContext);
}

void GpuScene::DisposeResources()
{
	for (auto& rtblas : m_RaytracingBottomLevelAccelerationStructures)
	{
		pRenderDevice->Destroy(rtblas.Scratch);
	}

	GpuTextureAllocator.DisposeResources();
}

void GpuScene::UploadLights()
{
	auto pLightTable = pRenderDevice->GetBuffer(m_LightTable);

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
	auto pMaterialTable = pRenderDevice->GetBuffer(m_MaterialTable);

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
	auto pMeshTable = pRenderDevice->GetBuffer(m_MeshTable);

	std::vector<HLSL::Mesh> hlslMeshes;
	for (auto& modelInstance : pScene->ModelInstances)
	{
		for (auto& meshInstance : modelInstance.MeshInstances)
		{
			// InstanceID will be used to find the data for this instance in shader
			meshInstance.InstanceID = hlslMeshes.size();

			hlslMeshes.push_back(GetHLSLMeshDesc(modelInstance.pMaterial->GpuMaterialIndex, meshInstance));
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

bool GpuScene::Update(float AspectRatio)
{
	pScene->Camera.SetAspectRatio(AspectRatio);

	bool Refresh = false;

	auto pLightTable = pRenderDevice->GetBuffer(m_LightTable);
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

	auto pMaterialTable = pRenderDevice->GetBuffer(m_MaterialTable);
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

	if (pScene->PreviousCamera != pScene->Camera)
	{
		Refresh |= true;
	}

	return Refresh;
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
			RTBLAS& RTBLAS = m_RaytracingBottomLevelAccelerationStructures[meshInstance.pMesh->BottomLevelAccelerationStructureIndex];
			Buffer* pBLAS = pRenderDevice->GetBuffer(RTBLAS.Result);

			RaytracingInstanceDesc Desc					= {};
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
	TopLevelAccelerationStructure.ComputeMemoryRequirements(&pRenderDevice->Device, &scratchSizeInBytes, &resultSizeInBytes, &instanceDescsSizeInBytes);

	Buffer* pScratch = pRenderDevice->GetBuffer(m_RaytracingTopLevelAccelerationStructure.Scratch);
	Buffer* pResult = pRenderDevice->GetBuffer(m_RaytracingTopLevelAccelerationStructure.Result);
	Buffer* pInstanceDescs = pRenderDevice->GetBuffer(m_RaytracingTopLevelAccelerationStructure.InstanceDescs);

	if (!pScratch || pScratch->GetSizeInBytes() < scratchSizeInBytes)
	{
		pRenderDevice->Destroy(m_RaytracingTopLevelAccelerationStructure.Scratch);

		// TLAS Scratch
		pRenderDevice->CreateBuffer(m_RaytracingTopLevelAccelerationStructure.Scratch, [=](BufferProxy& proxy)
		{
			proxy.SetSizeInBytes(scratchSizeInBytes);
			proxy.BindFlags = Resource::BindFlags::AccelerationStructure;
			proxy.InitialState = Resource::State::UnorderedAccess;
		});
		pScratch = pRenderDevice->GetBuffer(m_RaytracingTopLevelAccelerationStructure.Scratch);
	}

	if (!pResult || pResult->GetSizeInBytes() < resultSizeInBytes)
	{
		pRenderDevice->Destroy(m_RaytracingTopLevelAccelerationStructure.Result);

		// TLAS Result
		pRenderDevice->CreateBuffer(m_RaytracingTopLevelAccelerationStructure.Result, [=](BufferProxy& proxy)
		{
			proxy.SetSizeInBytes(resultSizeInBytes);
			proxy.BindFlags = Resource::BindFlags::AccelerationStructure;
			proxy.InitialState = Resource::State::AccelerationStructure;
		});
		pResult = pRenderDevice->GetBuffer(m_RaytracingTopLevelAccelerationStructure.Result);
	}

	if (!pInstanceDescs || pInstanceDescs->GetSizeInBytes() < instanceDescsSizeInBytes)
	{
		pRenderDevice->Destroy(m_RaytracingTopLevelAccelerationStructure.InstanceDescs);

		// TLAS Instance Desc
		pRenderDevice->CreateBuffer(m_RaytracingTopLevelAccelerationStructure.InstanceDescs, [=](BufferProxy& proxy)
		{
			proxy.SetSizeInBytes(instanceDescsSizeInBytes);
			proxy.SetCpuAccess(Buffer::CpuAccess::Write);
		});
		pInstanceDescs = pRenderDevice->GetBuffer(m_RaytracingTopLevelAccelerationStructure.InstanceDescs);
	}

	pResult->SetDebugName(L"Top Level Acceleration Structure");

	TopLevelAccelerationStructure.Generate(RenderContext.GetCommandContext(), pScratch, pResult, pInstanceDescs);
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
		rtblas.BLAS.ComputeMemoryRequirements(&pRenderDevice->Device, &scratchSizeInBytes, &resultSizeInBytes);

		// BLAS Scratch
		pRenderDevice->CreateBuffer(rtblas.Scratch, [=](BufferProxy& proxy)
		{
			proxy.SetSizeInBytes(scratchSizeInBytes);
			proxy.BindFlags = Resource::BindFlags::AccelerationStructure;
			proxy.InitialState = Resource::State::UnorderedAccess;
		});

		// BLAS Result
		pRenderDevice->CreateBuffer(rtblas.Result, [=](BufferProxy& proxy)
		{
			proxy.SetSizeInBytes(resultSizeInBytes);
			proxy.BindFlags = Resource::BindFlags::AccelerationStructure;
			proxy.InitialState = Resource::State::AccelerationStructure;
		});

		Buffer* pResult = pRenderDevice->GetBuffer(rtblas.Result);
		Buffer* pScratch = pRenderDevice->GetBuffer(rtblas.Scratch);

		rtblas.BLAS.Generate(RenderContext.GetCommandContext(), pScratch, pResult);
	}
}