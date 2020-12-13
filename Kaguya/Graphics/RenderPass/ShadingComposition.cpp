#include "pch.h"
#include "ShadingComposition.h"

#include "Graphics/Scene/Scene.h"
#include "Graphics/GpuScene.h"
#include "Graphics/RenderGraph.h"
#include "Graphics/RendererRegistry.h"

#include "Shading.h"
#include "SVGF.h"

ShadingComposition::ShadingComposition(UINT Width, UINT Height)
	: RenderPass("Shading Composition", { Width, Height }, NumResources)
{

}

void ShadingComposition::InitializePipeline(RenderDevice* pRenderDevice)
{
	Resources[RenderTarget] = pRenderDevice->InitializeRenderResourceHandle(RenderResourceType::Texture, "Render Pass[" + Name + "]: " + "RenderTarget");

	pRenderDevice->CreateComputePipelineState(ComputePSOs::ShadingComposition, [=](ComputePipelineStateBuilder& Builder)
	{
		Builder.pRootSignature = pRenderDevice->GetRootSignature(RootSignatures::Default);
		Builder.pCS = &Shaders::CS::ShadingComposition;
	});
}

void ShadingComposition::ScheduleResource(ResourceScheduler* pResourceScheduler, RenderGraph* pRenderGraph)
{
	pResourceScheduler->AllocateTexture(Resource::Type::Texture2D, [&](TextureProxy& proxy)
	{
		proxy.SetFormat(DXGI_FORMAT_R32G32B32A32_FLOAT);
		proxy.SetWidth(Properties.Width);
		proxy.SetHeight(Properties.Height);
		proxy.BindFlags = Resource::Flags::UnorderedAccess;
		proxy.InitialState = Resource::State::UnorderedAccess;
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
	} g_RenderPassData;

	g_RenderPassData.AnalyticUnshadowed	= RenderContext.GetShaderResourceView(pShadingRenderPass->Resources[Shading::EResources::AnalyticUnshadowed]).HeapIndex;
	g_RenderPassData.Source0Index		= RenderContext.GetShaderResourceView(pSVGFAtrousRenderPass->Resources[SVGFAtrous::EResources::RenderTarget0]).HeapIndex;
	g_RenderPassData.Source1Index		= RenderContext.GetShaderResourceView(pSVGFAtrousRenderPass->Resources[SVGFAtrous::EResources::RenderTarget1]).HeapIndex;

	g_RenderPassData.RenderTarget		= RenderContext.GetUnorderedAccessView(Resources[EResources::RenderTarget]).HeapIndex;

	RenderContext.UpdateRenderPassData<RenderPassData>(g_RenderPassData);

	RenderContext.SetPipelineState(ComputePSOs::ShadingComposition);
	RenderContext->Dispatch2D(Properties.Width, Properties.Height, 16, 16);
}

void ShadingComposition::StateRefresh()
{

}