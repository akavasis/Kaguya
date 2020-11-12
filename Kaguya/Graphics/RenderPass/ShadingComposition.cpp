#include "pch.h"
#include "ShadingComposition.h"

#include "Shading.h"
#include "ShadowDenoiser.h"

ShadingComposition::ShadingComposition(UINT Width, UINT Height)
	: RenderPass("Shading Composition",
		{ Width, Height, RendererFormats::HDRBufferFormat })
{

}

void ShadingComposition::InitializePipeline(RenderDevice* pRenderDevice)
{
	RootSignatures::ShadingComposition = pRenderDevice->CreateRootSignature([&](RootSignatureProxy& proxy)
	{
		proxy.DenyTessellationShaderAccess();
		proxy.DenyGSAccess();
	});

	ComputePSOs::ShadingComposition = pRenderDevice->CreateComputePipelineState([=](ComputePipelineStateProxy& proxy)
	{
		proxy.pRootSignature = pRenderDevice->GetRootSignature(RootSignatures::ShadingComposition);
		proxy.pCS = &Shaders::CS::ShadingComposition;
	});
}

void ShadingComposition::ScheduleResource(ResourceScheduler* pResourceScheduler)
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

void ShadingComposition::InitializeScene(GpuScene* pGpuScene, RenderDevice* pRenderDevice)
{

}

void ShadingComposition::RenderGui()
{

}

void ShadingComposition::Execute(RenderContext& RenderContext, RenderGraph* pRenderGraph)
{
	PIXMarker(RenderContext->GetD3DCommandList(), L"Shading Composition");

	auto pShadingRenderPass = pRenderGraph->GetRenderPass<Shading>();
	auto pShadowDenoiserRenderPass = pRenderGraph->GetRenderPass<ShadowDenoiser>();

	struct ShadingCompositionData
	{
		int AnalyticUnshadowed; // U
		int WeightedShadow;		// W_N

		int RenderTarget;
	} Data;
	Data.AnalyticUnshadowed		= RenderContext.GetShaderResourceView(pShadingRenderPass->Resources[Shading::EResources::AnalyticUnshadowed]).HeapIndex;
	Data.WeightedShadow			= RenderContext.GetShaderResourceView(pShadowDenoiserRenderPass->Resources[ShadowDenoiser::EResources::WeightedShadow]).HeapIndex;

	Data.RenderTarget			= RenderContext.GetUnorderedAccessView(Resources[EResources::RenderTarget]).HeapIndex;

	RenderContext.UpdateRenderPassData<ShadingCompositionData>(Data);

	RenderContext.TransitionBarrier(Resources[EResources::RenderTarget], DeviceResource::State::UnorderedAccess);

	RenderContext.SetPipelineState(ComputePSOs::ShadingComposition);

	RenderContext->Dispatch2D(Properties.Width, Properties.Height, 8, 8);

	RenderContext.UAVBarrier(Resources[EResources::RenderTarget]);
}

void ShadingComposition::StateRefresh()
{

}