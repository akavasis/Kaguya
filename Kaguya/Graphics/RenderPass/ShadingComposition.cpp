#include "pch.h"
#include "ShadingComposition.h"

#include "Shading.h"
#include "SVGF.h"

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

void ShadingComposition::ScheduleResource(ResourceScheduler* pResourceScheduler, RenderGraph* pRenderGraph)
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
	auto pSVGFAtrousRenderPass = pRenderGraph->GetRenderPass<SVGFAtrous>();

	struct RenderPassData
	{
		// Input Textures
		uint AnalyticUnshadowed;	// U
		uint Source0Index;			// S_N
		uint Source1Index;			// U_N

		// Output Textures
		uint RenderTarget;
	} Data;

	Data.AnalyticUnshadowed	= RenderContext.GetShaderResourceView(pShadingRenderPass->Resources[Shading::EResources::AnalyticUnshadowed]).HeapIndex;
	Data.Source0Index		= RenderContext.GetShaderResourceView(pSVGFAtrousRenderPass->Resources[SVGFAtrous::EResources::RenderTarget0]).HeapIndex;
	Data.Source1Index		= RenderContext.GetShaderResourceView(pSVGFAtrousRenderPass->Resources[SVGFAtrous::EResources::RenderTarget1]).HeapIndex;

	Data.RenderTarget		= RenderContext.GetUnorderedAccessView(Resources[EResources::RenderTarget]).HeapIndex;

	RenderContext.UpdateRenderPassData<RenderPassData>(Data);

	RenderContext.SetPipelineState(ComputePSOs::ShadingComposition);
	RenderContext->Dispatch2D(Properties.Width, Properties.Height, 8, 8);
}

void ShadingComposition::StateRefresh()
{

}