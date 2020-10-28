#include "pch.h"
#include "Accumulation.h"

#include "Pathtracing.h"
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
	pResourceScheduler->AllocateTexture(DeviceResource::Type::Texture2D, [&](DeviceTextureProxy& proxy)
	{
		proxy.SetFormat(Properties.Format);
		proxy.SetWidth(Properties.Width);
		proxy.SetHeight(Properties.Height);
		proxy.BindFlags = DeviceResource::BindFlags::UnorderedAccess;
		proxy.InitialState = DeviceResource::State::UnorderedAccess;
	});
}

void Accumulation::InitializeScene(GpuScene* pGpuScene, RenderDevice* pRenderDevice)
{
	this->pGpuScene = pGpuScene;

	LastFocalLength = pGpuScene->pScene->Camera.FocalLength;
	LastRelativeAperture = pGpuScene->pScene->Camera.RelativeAperture;
	LastCameraTransform = pGpuScene->pScene->Camera.Transform;
}

void Accumulation::RenderGui()
{

}

void Accumulation::Execute(RenderContext& RenderContext, RenderGraph* pRenderGraph)
{
	PIXMarker(RenderContext->GetD3DCommandList(), L"Accumulation");

	auto pPathtracingRenderPass = pRenderGraph->GetRenderPass<Pathtracing>();
	Descriptor InputSRV = RenderContext.GetShaderResourceView(pPathtracingRenderPass->Resources[Pathtracing::EResources::RenderTarget]);

	//auto pAmbientOcclusionRenderPass = pRenderGraph->GetRenderPass<AmbientOcclusion>();
	//Descriptor InputSRV = RenderContext.GetShaderResourceView(pAmbientOcclusionRenderPass->Resources[AmbientOcclusion::EResources::RenderTarget]);

	struct AccumulationData
	{
		uint InputIndex;
		uint OutputIndex;
	} Data;

	Data.InputIndex = InputSRV.HeapIndex;
	Data.OutputIndex = RenderContext.GetUnorderedAccessView(Resources[EResources::RenderTarget]).HeapIndex;
	RenderContext.UpdateRenderPassData<AccumulationData>(Data);

	// If the camera has moved
	if (pGpuScene->pScene->Camera.FocalLength != LastFocalLength ||
		pGpuScene->pScene->Camera.RelativeAperture != LastRelativeAperture ||
		pGpuScene->pScene->Camera.Transform != LastCameraTransform)
	{
		Settings.AccumulationCount = 0;
		LastFocalLength = pGpuScene->pScene->Camera.FocalLength;
		LastRelativeAperture = pGpuScene->pScene->Camera.RelativeAperture;
		LastCameraTransform = pGpuScene->pScene->Camera.Transform;
	}

	RenderContext.TransitionBarrier(Resources[EResources::RenderTarget], DeviceResource::State::UnorderedAccess);

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

	RenderContext.TransitionBarrier(Resources[EResources::RenderTarget], DeviceResource::State::NonPixelShaderResource);
}

void Accumulation::StateRefresh()
{
	Settings.AccumulationCount = 0;
}