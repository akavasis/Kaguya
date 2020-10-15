#include "pch.h"
#include "Accumulation.h"

#include "Pathtracing.h"
#include "RaytraceGBuffer.h"
#include "AmbientOcclusion.h"

Accumulation::Accumulation(UINT Width, UINT Height)
	: RenderPass("Accumulation",
		{ Width, Height, RendererFormats::HDRBufferFormat })
{

}

void Accumulation::InitializePipeline(RenderDevice* pRenderDevice)
{
	RootSignatures::Raytracing::Accumulation = pRenderDevice->CreateRootSignature([](RootSignatureProxy& proxy)
	{
		proxy.AddRootConstantsParameter(RootConstants<Accumulation::SSettings>(0, 0));

		proxy.DenyTessellationShaderAccess();
		proxy.DenyGSAccess();
	});

	ComputePSOs::Accumulation = pRenderDevice->CreateComputePipelineState([=](ComputePipelineStateProxy& proxy)
	{
		proxy.pRootSignature = pRenderDevice->GetRootSignature(RootSignatures::Raytracing::Accumulation);
		proxy.pCS = &Shaders::CS::Accumulation;
	});
}

void Accumulation::ScheduleResource(ResourceScheduler* pResourceScheduler)
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

void Accumulation::InitializeScene(GpuScene* pGpuScene, RenderDevice* pRenderDevice)
{
	this->pGpuScene = pGpuScene;

	LastGBuffer = 0;
	LastAperture = pGpuScene->pScene->Camera.Aperture;
	LastFocalLength = pGpuScene->pScene->Camera.FocalLength;
	LastCameraTransform = pGpuScene->pScene->Camera.Transform;
}

void Accumulation::RenderGui()
{

}

void Accumulation::Execute(RenderContext& RenderContext, RenderGraph* pRenderGraph)
{
	PIXMarker(RenderContext->GetD3DCommandList(), L"Accumulation");

	auto pPathtracingRenderPass = pRenderGraph->GetRenderPass<Pathtracing>();
	//auto pRaytraceGBufferRenderPass = pRenderGraph->GetRenderPass<RaytraceGBuffer>();
	//auto pAmbientOcclusionRenderPass = pRenderGraph->GetRenderPass<AmbientOcclusion>();

	struct AccumulationData
	{
		uint InputIndex;
		uint OutputIndex;
	} Data;

	Data.InputIndex = RenderContext.GetShaderResourceView(pPathtracingRenderPass->Resources[Pathtracing::EResources::RenderTarget]).HeapIndex;
	//Data.InputIndex = RenderContext.GetShaderResourceView(pRaytraceGBufferRenderPass->Resources[pRaytraceGBufferRenderPass->GetSettings().GBuffer]).HeapIndex;
	//Data.InputIndex = RenderContext.GetShaderResourceView(pAmbientOcclusionRenderPass->Resources[AmbientOcclusion::EResources::RenderTarget]).HeapIndex;
	Data.OutputIndex = RenderContext.GetUnorderedAccessView(Resources[EResources::RenderTarget]).HeapIndex;
	RenderContext.UpdateRenderPassData<AccumulationData>(Data);

	// If the camera has moved
	if (/*pRaytraceGBufferRenderPass->GetSettings().GBuffer != LastGBuffer ||*/
		pGpuScene->pScene->Camera.Aperture != LastAperture ||
		pGpuScene->pScene->Camera.FocalLength != LastFocalLength ||
		pGpuScene->pScene->Camera.Transform != LastCameraTransform)
	{
		Settings.AccumulationCount = 0;
		//LastGBuffer = pRaytraceGBufferRenderPass->GetSettings().GBuffer;
		LastAperture = pGpuScene->pScene->Camera.Aperture;
		LastFocalLength = pGpuScene->pScene->Camera.FocalLength;
		LastCameraTransform = pGpuScene->pScene->Camera.Transform;
	}

	RenderContext.TransitionBarrier(Resources[EResources::RenderTarget], Resource::State::UnorderedAccess);

	// Bind Pipeline
	RenderContext.SetPipelineState(ComputePSOs::Accumulation);

	// Bind Resources
	struct
	{
		unsigned int AccumulationCount;
	} AccumulationSettings;
	AccumulationSettings.AccumulationCount = Settings.AccumulationCount++;
	RenderContext->SetComputeRoot32BitConstants(0, 1, &AccumulationSettings, 0);

	RenderContext->Dispatch2D(Properties.Width, Properties.Height, 16, 16);

	RenderContext.TransitionBarrier(Resources[EResources::RenderTarget], Resource::State::NonPixelShaderResource);
}

void Accumulation::StateRefresh()
{
	Settings.AccumulationCount = 0;
}