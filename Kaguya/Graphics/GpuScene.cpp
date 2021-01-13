#include "pch.h"
#include "GpuScene.h"

#include "RendererRegistry.h"

#include <cstdio>

using namespace DirectX;

#define RAYTRACING_INSTANCEMASK_ALL 	(0xff)
#define RAYTRACING_INSTANCEMASK_OPAQUE 	(1 << 0)
#define RAYTRACING_INSTANCEMASK_LIGHT	(1 << 1)

namespace
{
	static constexpr size_t LightBufferByteSize			= Scene::MAX_LIGHT_SUPPORTED * sizeof(HLSL::PolygonalLight);
	static constexpr size_t MaterialBufferByteSize		= Scene::MAX_MATERIAL_SUPPORTED * sizeof(HLSL::Material);
	static constexpr size_t MeshBufferByteSize			= Scene::MAX_MESH_INSTANCE_SUPPORTED * sizeof(HLSL::Mesh);
	static constexpr size_t InstanceDescsSizeInBytes	= Scene::MAX_MESH_INSTANCE_SUPPORTED * sizeof(D3D12_RAYTRACING_INSTANCE_DESC);
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

HLSL::Mesh GetHLSLMeshDesc(const Mesh& Mesh, const Material& Material, const D3D12_RAYTRACING_INSTANCE_DESC& RayTracingInstanceDesc, MeshInstance& MeshInstance)
{
	XMMATRIX WorldMatrix = MeshInstance.Transform.Matrix();
	XMMATRIX PreviousWorldMatrix = MeshInstance.PreviousTransform.Matrix();

	matrix World, PreviousWorld;
	float3x4 Transform;

	XMStoreFloat4x4(&World, XMMatrixTranspose(WorldMatrix));
	XMStoreFloat4x4(&PreviousWorld, XMMatrixTranspose(PreviousWorldMatrix));
	XMStoreFloat3x4(&Transform, WorldMatrix);

	return
	{
		.VertexOffset							= Mesh.BaseVertexLocation,
		.IndexOffset							= Mesh.StartIndexLocation,
		.MaterialIndex							= MeshInstance.MaterialIndex,
		.InstanceID								= RayTracingInstanceDesc.InstanceID,
		.InstanceMask							= RayTracingInstanceDesc.InstanceMask,
		.InstanceContributionToHitGroupIndex	= RayTracingInstanceDesc.InstanceContributionToHitGroupIndex,
		.Flags									= RayTracingInstanceDesc.Flags,
		.AccelerationStructure					= RayTracingInstanceDesc.AccelerationStructure,
		.World									= World,
		.PreviousWorld							= PreviousWorld,
		.Transform								= Transform
	};
}

HLSL::Camera GetHLSLCameraDesc(const Camera& Camera)
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
		.NearZ						= Camera.NearZ,
		.FarZ						= Camera.FarZ,
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

bool EditTransform(
	const float* pCameraView,
	float* pCameraProjection,
	float* pMatrix,
	bool EditTransformDecomposition)
{
	bool IsEdited = false;

	static bool					UseSnap					= false;
	static float				Snap[3]					= { 1, 1, 1 };
	static ImGuizmo::OPERATION	CurrentGizmoOperation	= ImGuizmo::TRANSLATE;
	static ImGuizmo::MODE		CurrentGizmoMode		= ImGuizmo::WORLD;

	if (EditTransformDecomposition)
	{
		ImGui::Text("Operation");

		if (ImGui::RadioButton("Translate", CurrentGizmoOperation == ImGuizmo::TRANSLATE))
			CurrentGizmoOperation = ImGuizmo::TRANSLATE;
		ImGui::SameLine();

		if (ImGui::RadioButton("Rotate", CurrentGizmoOperation == ImGuizmo::ROTATE))
			CurrentGizmoOperation = ImGuizmo::ROTATE;
		ImGui::SameLine();

		if (ImGui::RadioButton("Scale", CurrentGizmoOperation == ImGuizmo::SCALE))
			CurrentGizmoOperation = ImGuizmo::SCALE;

		ImGui::Text("Transform");

		float matrixTranslation[3], matrixRotation[3], matrixScale[3];
		ImGuizmo::DecomposeMatrixToComponents(pMatrix, matrixTranslation, matrixRotation, matrixScale);
		IsEdited |= ImGui::InputFloat3("Translation", matrixTranslation);
		IsEdited |= ImGui::InputFloat3("Rotation", matrixRotation);
		IsEdited |= ImGui::InputFloat3("Scale", matrixScale);
		ImGuizmo::RecomposeMatrixFromComponents(matrixTranslation, matrixRotation, matrixScale, pMatrix);

		if (CurrentGizmoOperation != ImGuizmo::SCALE)
		{
			if (ImGui::RadioButton("Local", CurrentGizmoMode == ImGuizmo::LOCAL))
				CurrentGizmoMode = ImGuizmo::LOCAL;
			ImGui::SameLine();
			if (ImGui::RadioButton("World", CurrentGizmoMode == ImGuizmo::WORLD))
				CurrentGizmoMode = ImGuizmo::WORLD;
		}

		ImGui::Checkbox("", &UseSnap);
		ImGui::SameLine();

		switch (CurrentGizmoOperation)
		{
		case ImGuizmo::TRANSLATE:
			ImGui::InputFloat3("Snap", &Snap[0]);
			break;
		case ImGuizmo::ROTATE:
			ImGui::InputFloat("Angle Snap", &Snap[0]);
			break;
		case ImGuizmo::SCALE:
			ImGui::InputFloat("Scale Snap", &Snap[0]);
			break;
		}
	}

	IsEdited |= ImGuizmo::Manipulate(
		pCameraView, pCameraProjection,
		CurrentGizmoOperation, CurrentGizmoMode,
		pMatrix, nullptr,
		UseSnap ? Snap : nullptr);

	return IsEdited;
}

GpuScene::GpuScene(RenderDevice* pRenderDevice)
	: pRenderDevice(pRenderDevice),
	pScene(nullptr),
	BufferManager(pRenderDevice),
	TextureManager(pRenderDevice)
{
	m_LightTable = pRenderDevice->InitializeRenderResourceHandle(RenderResourceType::Buffer, "Light Table");
	m_MaterialTable = pRenderDevice->InitializeRenderResourceHandle(RenderResourceType::Buffer, "Material Table");
	m_InstanceDescsBuffer = pRenderDevice->InitializeRenderResourceHandle(RenderResourceType::Buffer, "Instance Descs Buffer");
	m_MeshTable = pRenderDevice->InitializeRenderResourceHandle(RenderResourceType::Buffer, "Mesh Table");

	m_RaytracingTopLevelAccelerationStructure =
	{
		pRenderDevice->InitializeRenderResourceHandle(RenderResourceType::Buffer, "Top-Level Acceleration Structure (Scratch)"),
		pRenderDevice->InitializeRenderResourceHandle(RenderResourceType::Buffer, "Top-Level Acceleration Structure")
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

	pRenderDevice->CreateBuffer(m_InstanceDescsBuffer, [&](BufferProxy& proxy)
	{
		proxy.SetSizeInBytes(InstanceDescsSizeInBytes);
		proxy.SetStride(sizeof(D3D12_RAYTRACING_INSTANCE_DESC));
		proxy.BindFlags = Resource::Flags::UnorderedAccess;
		proxy.InitialState = Resource::State::NonPixelShaderResource;
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

	BufferManager.DisposeResources();
	TextureManager.DisposeResources();
}

void GpuScene::SetSelectedInstanceID(INT SelectedInstanceID)
{
	m_SelectedInstanceID = SelectedInstanceID;
}

void GpuScene::RenderGui()
{
	if (ImGui::Begin("Scene"))
	{
		pScene->Camera.RenderGui();

		if (ImGui::TreeNode("Lights"))
		{
			for (auto& light : pScene->Lights)
			{
				light.RenderGui();
			}

			ImGui::TreePop();
		}

		if (ImGui::TreeNode("Materials"))
		{
			for (auto& material : pScene->Materials)
			{
				material.RenderGui();
			}

			ImGui::TreePop();
		}

		if (ImGui::TreeNode("Meshes"))
		{
			for (auto& mesh : pScene->Meshes)
			{
				mesh.RenderGui();
			}

			ImGui::TreePop();
		}

		if (ImGui::TreeNode("Mesh Instances"))
		{
			for (auto& meshInstance : pScene->MeshInstances)
			{
				meshInstance.RenderGui(pScene->Meshes.size(), pScene->Materials.size());
			}

			ImGui::TreePop();
		}
	}
	ImGui::End();

	ImGui::Begin("Editor");
	{
		// Render ImGuizmo
		MeshInstance* SelectedMeshInstance = nullptr;
		bool IsValidInstanceID = m_SelectedInstanceID != -1;
		if (IsValidInstanceID)
		{
			SelectedMeshInstance = &pScene->MeshInstances[m_SelectedInstanceID];
		}

		if (SelectedMeshInstance)
		{
			DirectX::XMFLOAT4X4 World;
			DirectX::XMFLOAT4X4 View, Projection;

			// Dont transpose this
			XMStoreFloat4x4(&World, SelectedMeshInstance->Transform.Matrix());
			XMStoreFloat4x4(&View, pScene->Camera.ViewMatrix());
			XMStoreFloat4x4(&Projection, pScene->Camera.ProjectionMatrix());

			ImGuiIO& io = ImGui::GetIO();
			ImGuizmo::SetRect(0, 0, io.DisplaySize.x, io.DisplaySize.y);
			ImGui::Separator();

			// I'm not sure what this does, I put it here because the example does it...
			ImGuizmo::SetID(0);

			// If we have edited the transform, update it and mark it as dirty so it will be updated on the GPU side
			if (EditTransform(
				reinterpret_cast<float*>(&View),
				reinterpret_cast<float*>(&Projection),
				reinterpret_cast<float*>(&World),
				SelectedMeshInstance != nullptr))
			{
				SelectedMeshInstance->Transform.SetTransform(XMLoadFloat4x4(&World));
				SelectedMeshInstance->Dirty = true;
			}
		}
	}
	ImGui::End();
}

bool GpuScene::Update(float AspectRatio)
{
	pScene->Camera.AspectRatio = AspectRatio;

	bool Refresh = false;

	auto pLightTable = pRenderDevice->GetBuffer(m_LightTable);
	pLightTable->Map();
	for (auto [i, light] : enumerate(pScene->Lights))
	{
		if (light.Dirty)
		{
			Refresh |= true;

			light.Dirty = false;

			auto hlslPolygonalLight = GetHLSLLightDesc(light);
			pLightTable->Update<HLSL::PolygonalLight>(i, hlslPolygonalLight);
		}
	}

	auto pMaterialTable = pRenderDevice->GetBuffer(m_MaterialTable);
	pMaterialTable->Map();
	for (auto [i, material] : enumerate(pScene->Materials))
	{
		if (material.Dirty)
		{
			Refresh |= true;

			material.Dirty = false;

			auto hlslMaterial = GetHLSLMaterialDesc(material);
			pMaterialTable->Update<HLSL::Material>(i, hlslMaterial);
		}
	}

	auto pMeshTable = pRenderDevice->GetBuffer(m_MeshTable);
	pMeshTable->Map();
	for (auto [i, meshInstance] : enumerate(pScene->MeshInstances))
	{
		const auto& mesh = pScene->Meshes[meshInstance.MeshIndex];
		const auto& material = pScene->Materials[meshInstance.MaterialIndex];

		meshInstance.InstanceID = i;

		RTBLAS& RTBLAS = m_RaytracingBottomLevelAccelerationStructures[mesh.BottomLevelAccelerationStructureIndex];
		Buffer* pBLAS = pRenderDevice->GetBuffer(RTBLAS.Result);

		D3D12_RAYTRACING_INSTANCE_DESC Desc			= {};
		Desc.InstanceID								= meshInstance.InstanceID;
		Desc.InstanceMask							= RAYTRACING_INSTANCEMASK_ALL;
		Desc.InstanceContributionToHitGroupIndex	= i;
		Desc.Flags									= D3D12_RAYTRACING_INSTANCE_FLAG_NONE;
		Desc.AccelerationStructure					= pBLAS->GetGpuVirtualAddress();

		if (meshInstance.Dirty)
		{
			Refresh |= true;

			meshInstance.Dirty = false;

			auto hlslMesh = GetHLSLMeshDesc(mesh, material, Desc, meshInstance);
			pMeshTable->Update<HLSL::Mesh>(i, hlslMesh);
		}
	}

	if (pScene->PreviousCamera != pScene->Camera)
	{
		Refresh |= true;
	}

	return Refresh;
}

void GpuScene::CreateTopLevelAS(CommandContext* pCommandContext)
{
	PIXScopedEvent(pCommandContext->GetApiHandle(), 0, L"Top Level Acceleration Structure Generation");

	UINT NumInstances = pScene->MeshInstances.size();

	m_TopLevelAccelerationStructure.SetNumInstances(NumInstances);

	auto pPipelineState = pRenderDevice->GetPipelineState(ComputePSOs::InstanceGeneration);
	auto pRootSignature = pRenderDevice->GetRootSignature(RootSignatures::InstanceGeneration);
	pCommandContext->SetPipelineState(pPipelineState);
	pCommandContext->SetComputeRootSignature(pRootSignature);

	auto pMeshBuffer = pRenderDevice->GetBuffer(m_MeshTable);
	auto pInstanceDescsBuffer = pRenderDevice->GetBuffer(m_InstanceDescsBuffer);
	pCommandContext->SetComputeRoot32BitConstant(0, NumInstances, 0);
	pCommandContext->SetComputeRootShaderResourceView(1, pMeshBuffer->GetGpuVirtualAddress());
	pCommandContext->SetComputeRootUnorderedAccessView(2, pInstanceDescsBuffer->GetGpuVirtualAddress());

	pCommandContext->Dispatch1D(NumInstances, 64);
	pCommandContext->UAVBarrier(pInstanceDescsBuffer);
	pCommandContext->FlushResourceBarriers();

	UINT64 ScratchSizeInBytes, ResultSizeInBytes;
	m_TopLevelAccelerationStructure.ComputeMemoryRequirements(pRenderDevice->Device, &ScratchSizeInBytes, &ResultSizeInBytes);

	Buffer* pScratch = pRenderDevice->GetBuffer(m_RaytracingTopLevelAccelerationStructure.Scratch);
	Buffer* pResult = pRenderDevice->GetBuffer(m_RaytracingTopLevelAccelerationStructure.Result);

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

	m_TopLevelAccelerationStructure.Generate(pCommandContext, pScratch, pResult, pInstanceDescsBuffer);
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

		D3D12_RAYTRACING_GEOMETRY_DESC Desc			= {};
		Desc.Type									= D3D12_RAYTRACING_GEOMETRY_TYPE_TRIANGLES;
		Desc.Flags									= D3D12_RAYTRACING_GEOMETRY_FLAG_OPAQUE;
		Desc.Triangles.Transform3x4					= NULL;
		Desc.Triangles.IndexFormat					= DXGI_FORMAT_R32_UINT;
		Desc.Triangles.VertexFormat					= DXGI_FORMAT_R32G32B32_FLOAT; // Position attribute of the vertex
		Desc.Triangles.IndexCount					= Mesh.IndexCount;
		Desc.Triangles.VertexCount					= Mesh.VertexCount;
		Desc.Triangles.IndexBuffer					= pIndexBuffer->GetGpuVirtualAddress() + Mesh.StartIndexLocation * sizeof(unsigned int);
		Desc.Triangles.VertexBuffer.StartAddress	= pVertexBuffer->GetGpuVirtualAddress() + Mesh.BaseVertexLocation * sizeof(Vertex);
		Desc.Triangles.VertexBuffer.StrideInBytes	= sizeof(Vertex);

		rtblas.BottomLevelAccelerationStructure.AddGeometry(Desc);

		m_RaytracingBottomLevelAccelerationStructures.push_back(rtblas);
	}

	PIXScopedEvent(RenderContext->GetApiHandle(), 0, L"Bottom Level Acceleration Structure Generation");

	for (auto& rtblas : m_RaytracingBottomLevelAccelerationStructures)
	{
		UINT64 scratchSizeInBytes, resultSizeInBytes;
		rtblas.BottomLevelAccelerationStructure.ComputeMemoryRequirements(pRenderDevice->Device, &scratchSizeInBytes, &resultSizeInBytes);

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

		rtblas.BottomLevelAccelerationStructure.Generate(RenderContext.GetCommandContext(), pScratch, pResult);
	}
}