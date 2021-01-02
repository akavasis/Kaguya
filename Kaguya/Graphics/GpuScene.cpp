#include "pch.h"
#include "GpuScene.h"

using namespace DirectX;

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
}

HLSL::PolygonalLight GetHLSLLightDesc(const PolygonalLight& Light)
{
	XMMATRIX Transform = Light.Transform.Matrix();

	matrix World;
	XMStoreFloat4x4(&World, XMMatrixTranspose(Transform));
	float4 Orientation;
	XMStoreFloat4(&Orientation, DirectX::XMVector3Normalize(Light.Transform.Forward()));
	float3 Points[4];
	float halfWidth = Light.Width * 0.5f;
	float halfHeight = Light.Height * 0.5f;
	// Get billboard points at the origin
	XMVECTOR p0 = XMVectorSet(+halfWidth, -halfHeight, 0, 1);
	XMVECTOR p1 = XMVectorSet(+halfWidth, +halfHeight, 0, 1);
	XMVECTOR p2 = XMVectorSet(-halfWidth, +halfHeight, 0, 1);
	XMVECTOR p3 = XMVectorSet(-halfWidth, -halfHeight, 0, 1);

	// Precompute the light points here so ray generation shader doesnt have to do it
	// for every ray
	// Move points to light's location
	// Clockwise to match LTC convention
	float3 points[4];
	XMStoreFloat3(&points[0], XMVector3TransformCoord(p3, Transform));
	XMStoreFloat3(&points[1], XMVector3TransformCoord(p2, Transform));
	XMStoreFloat3(&points[2], XMVector3TransformCoord(p1, Transform));
	XMStoreFloat3(&points[3], XMVector3TransformCoord(p0, Transform));
	
	return
	{
		.Position		= Light.Transform.Position,
		.Orientation	= Orientation,
		.World			= World,
		.Color			= Light.Color,
		.Luminance		= Light.Luminance,
		.Width			= Light.Width,
		.Height			= Light.Height,
		.Points =
		{
			points[0],
			points[1],
			points[2],
			points[3],
		}
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
		.Model					= (uint)Material.Model,
		.UseAttributeAsValues	= (uint)Material.UseAttributes, // If this is true, then the attributes above will be used rather than actual textures
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

HLSL::Mesh GetHLSLMeshDesc(const Mesh& Mesh, const Material& Material, const MeshInstance& MeshInstance)
{
	matrix World;
	XMStoreFloat4x4(&World, XMMatrixTranspose(MeshInstance.Transform.Matrix()));
	matrix PreviousWorld;
	XMStoreFloat4x4(&PreviousWorld, XMMatrixTranspose(MeshInstance.PreviousTransform.Matrix()));
	return
	{
		.VertexOffset = Mesh.BaseVertexLocation,
		.IndexOffset = Mesh.StartIndexLocation,
		.MaterialIndex = (uint32_t)MeshInstance.MaterialIndex,
		.World = World,
		.PreviousWorld = PreviousWorld
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

GpuScene::GpuScene(RenderDevice* pRenderDevice)
	: pRenderDevice(pRenderDevice),
	pScene(nullptr),
	BufferManager(pRenderDevice),
	TextureManager(pRenderDevice)
{
	m_LightTable = pRenderDevice->InitializeRenderResourceHandle(RenderResourceType::Buffer, "Light Table");
	m_MaterialTable = pRenderDevice->InitializeRenderResourceHandle(RenderResourceType::Buffer, "Material Table");
	m_MeshTable = pRenderDevice->InitializeRenderResourceHandle(RenderResourceType::Buffer, "Mesh Table");

	m_RaytracingTopLevelAccelerationStructure =
	{
		pRenderDevice->InitializeRenderResourceHandle(RenderResourceType::Buffer, "Top-Level Acceleration Structure (Scratch)"),
		pRenderDevice->InitializeRenderResourceHandle(RenderResourceType::Buffer, "Top-Level Acceleration Structure"),
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
}

void GpuScene::UploadTextures(RenderContext& RenderContext)
{
	TextureManager.Stage(*pScene, RenderContext);
}

void GpuScene::UploadModels(RenderContext& RenderContext)
{
	BufferManager.Stage(*pScene, RenderContext);

	CreateBottomLevelAS(RenderContext);
}

void GpuScene::DisposeResources()
{
	for (auto& rtblas : m_RaytracingBottomLevelAccelerationStructures)
	{
		pRenderDevice->Destroy(rtblas.Scratch);
	}

	TextureManager.DisposeResources();
}

void GpuScene::UploadLights()
{
	auto pLightTable = pRenderDevice->GetBuffer(m_LightTable);

	std::vector<HLSL::PolygonalLight> hlslLights;
	hlslLights.reserve(pScene->Lights.size());

	for (auto [i, light] : enumerate(pScene->Lights))
	{
		hlslLights.push_back(GetHLSLLightDesc(light));
	}

	auto pGPU = pLightTable->Map();
	auto pCPU = hlslLights.data();
	memcpy(pGPU, pCPU, hlslLights.size() * sizeof(HLSL::PolygonalLight));
}

void GpuScene::UploadMaterials()
{
	auto pMaterialTable = pRenderDevice->GetBuffer(m_MaterialTable);

	std::vector<HLSL::Material> hlslMaterials(pScene->Materials.size());

	for (auto [i, material] : enumerate(pScene->Materials))
	{
		hlslMaterials[i] = GetHLSLMaterialDesc(material);
	}

	auto pGPU = pMaterialTable->Map();
	auto pCPU = hlslMaterials.data();
	memcpy(pGPU, pCPU, hlslMaterials.size() * sizeof(HLSL::Material));
}

void GpuScene::UploadMeshes()
{
	auto pMeshTable = pRenderDevice->GetBuffer(m_MeshTable);

	std::vector<HLSL::Mesh> hlslMeshes(pScene->MeshInstances.size());

	for (auto [i, meshInstance] : enumerate(pScene->MeshInstances))
	{
		// InstanceID will be used to find the data for this instance in shader
		meshInstance.InstanceID = i;

		const auto& mesh = pScene->Meshes[meshInstance.MeshIndex];
		const auto& material = pScene->Materials[meshInstance.MaterialIndex];

		hlslMeshes[i] = GetHLSLMeshDesc(mesh, material, meshInstance);
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
	for (auto [i, light] : enumerate(pScene->Lights))
	{
		if (light.Dirty)
		{
			Refresh |= true;

			light.Dirty = false;

			HLSL::PolygonalLight hlslPolygonalLight = GetHLSLLightDesc(light);
			pLightTable->Update<HLSL::PolygonalLight>(i, hlslPolygonalLight);
		}
	}

	auto pMaterialTable = pRenderDevice->GetBuffer(m_MaterialTable);
	for (auto [i, material] : enumerate(pScene->Materials))
	{
		if (material.Dirty)
		{
			Refresh |= true;

			material.Dirty = false;

			HLSL::Material hlslMaterial = GetHLSLMaterialDesc(material);
			pMaterialTable->Update<HLSL::Material>(i, hlslMaterial);
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
	PIXScopedEvent(RenderContext->GetApiHandle(), 0, L"Top Level Acceleration Structure Generation");

	TopLevelAccelerationStructure TopLevelAccelerationStructure;

	size_t HitGroupIndex = 0;
	for (const auto& meshInstance : pScene->MeshInstances)
	{
		const auto& mesh = pScene->Meshes[meshInstance.MeshIndex];

		RTBLAS& RTBLAS = m_RaytracingBottomLevelAccelerationStructures[mesh.BottomLevelAccelerationStructureIndex];
		Buffer* pBLAS = pRenderDevice->GetBuffer(RTBLAS.Result);

		RaytracingInstanceDesc Desc					= {};
		Desc.AccelerationStructure					= pBLAS->GetGpuVirtualAddress();
		Desc.Transform								= meshInstance.Transform.Matrix();
		Desc.InstanceID								= meshInstance.InstanceID;
		Desc.InstanceMask							= RAYTRACING_INSTANCEMASK_ALL;

		Desc.InstanceContributionToHitGroupIndex	= HitGroupIndex;

		TopLevelAccelerationStructure.AddInstance(Desc);

		HitGroupIndex++;
	}

	UINT64 ScratchSizeInBytes, ResultSizeInBytes, InstanceDescsSizeInBytes;
	TopLevelAccelerationStructure.ComputeMemoryRequirements(pRenderDevice->Device, &ScratchSizeInBytes, &ResultSizeInBytes, &InstanceDescsSizeInBytes);

	Buffer* pScratch = pRenderDevice->GetBuffer(m_RaytracingTopLevelAccelerationStructure.Scratch);
	Buffer* pResult = pRenderDevice->GetBuffer(m_RaytracingTopLevelAccelerationStructure.Result);
	Buffer* pInstanceDescs = pRenderDevice->GetBuffer(m_RaytracingTopLevelAccelerationStructure.InstanceDescs);

	if (!pScratch || pScratch->GetSizeInBytes() < ScratchSizeInBytes)
	{
		pRenderDevice->Destroy(m_RaytracingTopLevelAccelerationStructure.Scratch);

		// TLAS Scratch
		pRenderDevice->CreateBuffer(m_RaytracingTopLevelAccelerationStructure.Scratch, [=](BufferProxy& proxy)
		{
			proxy.SetSizeInBytes(ScratchSizeInBytes);
			proxy.BindFlags = Resource::Flags::AccelerationStructure;
			proxy.InitialState = Resource::State::UnorderedAccess;
		});
		pScratch = pRenderDevice->GetBuffer(m_RaytracingTopLevelAccelerationStructure.Scratch);
	}

	if (!pResult || pResult->GetSizeInBytes() < ResultSizeInBytes)
	{
		pRenderDevice->Destroy(m_RaytracingTopLevelAccelerationStructure.Result);

		// TLAS Result
		pRenderDevice->CreateBuffer(m_RaytracingTopLevelAccelerationStructure.Result, [=](BufferProxy& proxy)
		{
			proxy.SetSizeInBytes(ResultSizeInBytes);
			proxy.BindFlags = Resource::Flags::AccelerationStructure;
			proxy.InitialState = Resource::State::AccelerationStructure;
		});
		pResult = pRenderDevice->GetBuffer(m_RaytracingTopLevelAccelerationStructure.Result);
	}

	if (!pInstanceDescs || pInstanceDescs->GetSizeInBytes() < InstanceDescsSizeInBytes)
	{
		pRenderDevice->Destroy(m_RaytracingTopLevelAccelerationStructure.InstanceDescs);

		// TLAS Instance Desc
		pRenderDevice->CreateBuffer(m_RaytracingTopLevelAccelerationStructure.InstanceDescs, [=](BufferProxy& proxy)
		{
			proxy.SetSizeInBytes(InstanceDescsSizeInBytes);
			proxy.SetCpuAccess(Buffer::CpuAccess::Write);
		});
		pInstanceDescs = pRenderDevice->GetBuffer(m_RaytracingTopLevelAccelerationStructure.InstanceDescs);
	}

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
	for (auto& Mesh : pScene->Meshes)
	{
		auto pVertexBuffer = pRenderDevice->GetBuffer(Mesh.VertexResource);
		auto pIndexBuffer = pRenderDevice->GetBuffer(Mesh.IndexResource);

		RTBLAS rtblas;
		rtblas.Scratch = pRenderDevice->InitializeRenderResourceHandle(RenderResourceType::Buffer, Mesh.Name + " (Scratch)");
		rtblas.Result = pRenderDevice->InitializeRenderResourceHandle(RenderResourceType::Buffer, Mesh.Name + " (Result)");

		// Update mesh's BLAS Index
		Mesh.BottomLevelAccelerationStructureIndex = m_RaytracingBottomLevelAccelerationStructures.size();

		RaytracingGeometryDesc Desc = {};
		Desc.pVertexBuffer			= pVertexBuffer;
		Desc.VertexStride			= sizeof(Vertex);
		Desc.pIndexBuffer			= pIndexBuffer;
		Desc.IndexStride			= sizeof(unsigned int);
		Desc.IsOpaque				= true;
		Desc.NumVertices			= Mesh.VertexCount;
		Desc.VertexOffset			= Mesh.BaseVertexLocation;
		Desc.NumIndices				= Mesh.IndexCount;
		Desc.IndexOffset			= Mesh.StartIndexLocation;

		rtblas.BLAS.AddGeometry(Desc);

		m_RaytracingBottomLevelAccelerationStructures.push_back(rtblas);
	}

	PIXScopedEvent(RenderContext->GetApiHandle(), 0, L"Bottom Level Acceleration Structure Generation");

	for (auto& rtblas : m_RaytracingBottomLevelAccelerationStructures)
	{
		UINT64 scratchSizeInBytes, resultSizeInBytes;
		rtblas.BLAS.ComputeMemoryRequirements(pRenderDevice->Device, &scratchSizeInBytes, &resultSizeInBytes);

		// BLAS Scratch
		pRenderDevice->CreateBuffer(rtblas.Scratch, [=](BufferProxy& proxy)
		{
			proxy.SetSizeInBytes(scratchSizeInBytes);
			proxy.BindFlags = Resource::Flags::AccelerationStructure;
			proxy.InitialState = Resource::State::UnorderedAccess;
		});

		// BLAS Result
		pRenderDevice->CreateBuffer(rtblas.Result, [=](BufferProxy& proxy)
		{
			proxy.SetSizeInBytes(resultSizeInBytes);
			proxy.BindFlags = Resource::Flags::AccelerationStructure;
			proxy.InitialState = Resource::State::AccelerationStructure;
		});

		Buffer* pResult = pRenderDevice->GetBuffer(rtblas.Result);
		Buffer* pScratch = pRenderDevice->GetBuffer(rtblas.Scratch);

		rtblas.BLAS.Generate(RenderContext.GetCommandContext(), pScratch, pResult);
	}
}