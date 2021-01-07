#include "pch.h"
#include "GpuScene.h"

#include <cstdio>

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
	bool IsDirty = false;

	static bool					UseSnap					= false;
	static float				Snap[3]					= { 1, 1, 1 };
	static ImGuizmo::OPERATION	CurrentGizmoOperation	= ImGuizmo::TRANSLATE;
	static ImGuizmo::MODE		CurrentGizmoMode		= ImGuizmo::LOCAL;

	if (EditTransformDecomposition)
	{
		if (ImGui::RadioButton("Translate", CurrentGizmoOperation == ImGuizmo::TRANSLATE))
			CurrentGizmoOperation = ImGuizmo::TRANSLATE;
		ImGui::SameLine();

		if (ImGui::RadioButton("Rotate", CurrentGizmoOperation == ImGuizmo::ROTATE))
			CurrentGizmoOperation = ImGuizmo::ROTATE;
		ImGui::SameLine();

		if (ImGui::RadioButton("Scale", CurrentGizmoOperation == ImGuizmo::SCALE))
			CurrentGizmoOperation = ImGuizmo::SCALE;

		float matrixTranslation[3], matrixRotation[3], matrixScale[3];
		ImGuizmo::DecomposeMatrixToComponents(pMatrix, matrixTranslation, matrixRotation, matrixScale);
		IsDirty |= ImGui::InputFloat3("Tr", matrixTranslation);
		IsDirty |= ImGui::InputFloat3("Rt", matrixRotation);
		IsDirty |= ImGui::InputFloat3("Sc", matrixScale);
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

	IsDirty |= ImGuizmo::Manipulate(
		pCameraView,
		pCameraProjection,
		CurrentGizmoOperation,
		CurrentGizmoMode,
		pMatrix);

		LOG_INFO("{}", IsDirty);
	return IsDirty;
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
		bool IsValidInstanceID = m_SelectedInstanceID != -1;
		if (IsValidInstanceID)
		{
			m_SelectedMeshInstance = &pScene->MeshInstances[m_SelectedInstanceID];
		}

		if (m_SelectedMeshInstance)
		{
			DirectX::XMFLOAT4X4 World;
			DirectX::XMFLOAT4X4 View, Projection;

			// Dont transpose this
			XMStoreFloat4x4(&World, m_SelectedMeshInstance->Transform.Matrix());
			XMStoreFloat4x4(&View, pScene->Camera.ViewMatrix());
			XMStoreFloat4x4(&Projection, pScene->Camera.ProjectionMatrix());

			ImGuiIO& io = ImGui::GetIO();
			ImGuizmo::SetRect(0, 0, io.DisplaySize.x, io.DisplaySize.y);
			ImGui::Separator();

			ImGuizmo::SetID(0);

			if (EditTransform(
				reinterpret_cast<float*>(&View),
				reinterpret_cast<float*>(&Projection),
				reinterpret_cast<float*>(&World),
				m_SelectedMeshInstance != nullptr))
			{
				m_SelectedMeshInstance->Transform.SetTransform(XMLoadFloat4x4(&World));
				m_SelectedMeshInstance->Dirty = true;
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
		meshInstance.InstanceID = i;

		if (meshInstance.Dirty)
		{
			Refresh |= true;

			meshInstance.Dirty = false;

			const auto& mesh = pScene->Meshes[meshInstance.MeshIndex];
			const auto& material = pScene->Materials[meshInstance.MaterialIndex];

			auto hlslMesh = GetHLSLMeshDesc(mesh, material, meshInstance);
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

	// TODO: Generate InstanceDescs on CS
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

	TopLevelAccelerationStructure.Generate(pCommandContext, pScratch, pResult, pInstanceDescs);
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