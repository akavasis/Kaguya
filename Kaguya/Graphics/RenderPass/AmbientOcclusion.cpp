#include "pch.h"
#include "AmbientOcclusion.h"
#include "../Renderer.h"

#include "RaytraceGBuffer.h"

AmbientOcclusion::AmbientOcclusion(UINT Width, UINT Height)
	: RenderPass("Ambient Occlusion",
		{ Width, Height, RendererFormats::HDRBufferFormat })
{

}

AmbientOcclusion::~AmbientOcclusion()
{

}

void AmbientOcclusion::ScheduleResource(ResourceScheduler* pResourceScheduler)
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

void AmbientOcclusion::InitializeScene(GpuScene* pGpuScene, RenderDevice* pRenderDevice)
{
	this->pGpuScene = pGpuScene;

	RaytracingPipelineState* pRaytracingPipelineState = pRenderDevice->GetRaytracingPSO(RaytracingPSOs::AmbientOcclusion);

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

void AmbientOcclusion::RenderGui()
{
	if (ImGui::TreeNode(Name.data()))
	{
		int Dirty = 0;
		if (ImGui::Button("Restore Defaults"))
		{
			Settings = SSettings();
			Refresh = true;
		}
		Dirty |= (int)ImGui::DragFloat("AO Radius", &Settings.AORadius, Settings.AORadius * 0.01f, 1e-4f, 1e38f);
		Dirty |= (int)ImGui::SliderInt("Num AO Rays Per Pixel", &Settings.NumAORaysPerPixel, 1, 64);
		
		if (Dirty) Refresh = true;
		ImGui::TreePop();
	}
}

void AmbientOcclusion::Execute(RenderContext& RenderContext, RenderGraph* pRenderGraph)
{
	PIXMarker(RenderContext->GetD3DCommandList(), L"Ambient Occlusion");

	auto pRaytraceGBufferRenderPass = pRenderGraph->GetRenderPass<RaytraceGBuffer>();

	struct AmbientOcclusionData
	{
		GlobalConstants GlobalConstants;
		float AORadius;
		int NumAORaysPerPixel;

		int InputWorldPositionIndex;
		int InputWorldNormalIndex;
		int OutputIndex;
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
	Data.AORadius = Settings.AORadius;
	Data.NumAORaysPerPixel = Settings.NumAORaysPerPixel;

	Data.InputWorldPositionIndex = RenderContext.GetSRV(pRaytraceGBufferRenderPass->Resources[RaytraceGBuffer::EResources::WorldPosition]).HeapIndex;
	Data.InputWorldNormalIndex = RenderContext.GetSRV(pRaytraceGBufferRenderPass->Resources[RaytraceGBuffer::EResources::WorldNormal]).HeapIndex;
	Data.OutputIndex = RenderContext.GetUAV(Resources[EResources::RenderTarget]).HeapIndex;
	RenderContext.UpdateRenderPassData<AmbientOcclusionData>(Data);

	RenderContext.TransitionBarrier(Resources[EResources::RenderTarget], Resource::State::UnorderedAccess);

	RenderContext.SetPipelineState(RaytracingPSOs::AmbientOcclusion);
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

	RenderContext.UAVBarrier(Resources[EResources::RenderTarget]);
}

void AmbientOcclusion::StateRefresh()
{

}