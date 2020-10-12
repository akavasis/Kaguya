#include "pch.h"
#include "RaytraceGBuffer.h"
#include "../Renderer.h"

RaytraceGBuffer::RaytraceGBuffer(UINT Width, UINT Height)
	: RenderPass("Raytrace GBuffer",
		{ Width, Height, DXGI_FORMAT_R32G32B32A32_FLOAT })
{

}

void RaytraceGBuffer::ScheduleResource(ResourceScheduler* pResourceScheduler)
{
	for (size_t i = 0; i < EResources::NumResources; ++i)
	{
		pResourceScheduler->AllocateTexture(Resource::Type::Texture2D, [&](TextureProxy& proxy)
		{
			proxy.SetFormat(Properties.Format);
			proxy.SetWidth(Properties.Width);
			proxy.SetHeight(Properties.Height);
			proxy.BindFlags = Resource::BindFlags::UnorderedAccess;
			proxy.InitialState = Resource::State::UnorderedAccess;
		});
	}
}

void RaytraceGBuffer::InitializeScene(GpuScene* pGpuScene, RenderDevice* pRenderDevice)
{
	this->pGpuScene = pGpuScene;

	RaytracingPipelineState* pRaytracingPipelineState = pRenderDevice->GetRaytracingPSO(RaytracingPSOs::RaytraceGBuffer);

	// Ray Generation Shader Table
	{
		ShaderTable<void> shaderTable;
		shaderTable.AddShaderRecord(pRaytracingPipelineState->GetShaderIdentifier(L"RayGeneration"));

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
		ShaderTable<void> shaderTable;

		ShaderIdentifier hitGroupSID = pRaytracingPipelineState->GetShaderIdentifier(L"Default");

		for (const auto& modelInstance : pGpuScene->pScene->ModelInstances)
		{
			for (const auto& meshInstance : modelInstance.MeshInstances)
			{
				shaderTable.AddShaderRecord(hitGroupSID);
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

void RaytraceGBuffer::RenderGui()
{
	if (ImGui::TreeNode(Name.data()))
	{
		if (ImGui::Button("Restore Defaults"))
		{
			Settings = SSettings();
		}

		const char* GBufferOutputs[] = { "Position", "Normal", "Albedo", "Emissive", "Specular", "Refraction", "Extra" };
		ImGui::Combo("GBuffer Outputs", &Settings.GBuffer, GBufferOutputs, ARRAYSIZE(GBufferOutputs), ARRAYSIZE(GBufferOutputs));

		ImGui::TreePop();
	}
}

void RaytraceGBuffer::Execute(RenderContext& RenderContext, RenderGraph* pRenderGraph)
{
	PIXMarker(RenderContext->GetD3DCommandList(), L"Raytrace GBuffer");

	struct RaytraceGBufferData
	{
		GlobalConstants GlobalConstants;
		int OutputWorldPositionIndex;
		int OutputWorldNormalIndex;
		int OutputMaterialAlbedoIndex;
		int OutputMaterialEmissiveIndex;
		int OutputMaterialSpecularIndex;
		int OutputMaterialRefractionIndex;
		int OutputMaterialExtraIndex;
	} Data;

	GlobalConstants globalConstants;
	XMStoreFloat3(&globalConstants.CameraU, pGpuScene->pScene->Camera.GetUVector());
	XMStoreFloat3(&globalConstants.CameraV, pGpuScene->pScene->Camera.GetVVector());
	XMStoreFloat3(&globalConstants.CameraW, pGpuScene->pScene->Camera.GetWVector());
	XMStoreFloat4x4(&globalConstants.View, XMMatrixTranspose(pGpuScene->pScene->Camera.ViewMatrix()));
	XMStoreFloat4x4(&globalConstants.Projection, XMMatrixTranspose(pGpuScene->pScene->Camera.ProjectionMatrix()));
	XMStoreFloat4x4(&globalConstants.InvView, XMMatrixTranspose(pGpuScene->pScene->Camera.InverseViewMatrix()));
	XMStoreFloat4x4(&globalConstants.InvProjection, XMMatrixTranspose(pGpuScene->pScene->Camera.InverseProjectionMatrix()));
	XMStoreFloat4x4(&globalConstants.ViewProjection, XMMatrixTranspose(pGpuScene->pScene->Camera.ViewProjectionMatrix()));
	globalConstants.EyePosition = pGpuScene->pScene->Camera.Transform.Position;
	globalConstants.TotalFrameCount = static_cast<unsigned int>(Renderer::Statistics::TotalFrameCount);

	globalConstants.Sun = pGpuScene->pScene->Sun;

	globalConstants.MaxDepth = 1;
	globalConstants.FocalLength = pGpuScene->pScene->Camera.FocalLength;
	globalConstants.LensRadius = pGpuScene->pScene->Camera.Aperture;

	Data.GlobalConstants = globalConstants;
	Data.OutputWorldPositionIndex = RenderContext.GetUnorderedAccessView(Resources[EResources::WorldPosition]).HeapIndex;
	Data.OutputWorldNormalIndex = RenderContext.GetUnorderedAccessView(Resources[EResources::WorldNormal]).HeapIndex;
	Data.OutputMaterialAlbedoIndex = RenderContext.GetUnorderedAccessView(Resources[EResources::MaterialAlbedo]).HeapIndex;
	Data.OutputMaterialEmissiveIndex = RenderContext.GetUnorderedAccessView(Resources[EResources::MaterialEmissive]).HeapIndex;
	Data.OutputMaterialSpecularIndex = RenderContext.GetUnorderedAccessView(Resources[EResources::MaterialSpecular]).HeapIndex;
	Data.OutputMaterialRefractionIndex = RenderContext.GetUnorderedAccessView(Resources[EResources::MaterialRefraction]).HeapIndex;
	Data.OutputMaterialExtraIndex = RenderContext.GetUnorderedAccessView(Resources[EResources::MaterialExtra]).HeapIndex;

	RenderContext.UpdateRenderPassData<RaytraceGBufferData>(Data);

	RenderContext.SetPipelineState(RaytracingPSOs::RaytraceGBuffer);
	RenderContext.SetRootShaderResourceView(0, pGpuScene->GetRTTLASResourceHandle());
	RenderContext.SetRootShaderResourceView(1, pGpuScene->GetVertexBufferHandle());
	RenderContext.SetRootShaderResourceView(2, pGpuScene->GetIndexBufferHandle());
	RenderContext.SetRootShaderResourceView(3, pGpuScene->GetGeometryInfoTableHandle());
	RenderContext.SetRootShaderResourceView(4, pGpuScene->GetMaterialTableHandle());

	RenderContext.DispatchRays(
		m_RayGenerationShaderTable,
		m_MissShaderTable,
		m_HitGroupShaderTable,
		Properties.Width,
		Properties.Height);

	RenderContext.UAVBarrier(Resources[EResources::WorldPosition]);
	RenderContext.UAVBarrier(Resources[EResources::WorldNormal]);
	RenderContext.UAVBarrier(Resources[EResources::MaterialAlbedo]);
	RenderContext.UAVBarrier(Resources[EResources::MaterialEmissive]);
	RenderContext.UAVBarrier(Resources[EResources::MaterialSpecular]);
	RenderContext.UAVBarrier(Resources[EResources::MaterialRefraction]);
	RenderContext.UAVBarrier(Resources[EResources::MaterialExtra]);
}

void RaytraceGBuffer::StateRefresh()
{

}