#include "pch.h"
#include "Accumulation.h"

#include "Graphics/Scene/Scene.h"
#include "Graphics/GpuScene.h"
#include "Graphics/RenderGraph.h"
#include "Graphics/RendererRegistry.h"

#include "Pathtracing.h"
#include "AmbientOcclusion.h"
#include "ShadingComposition.h"

Accumulation::Accumulation(UINT Width, UINT Height)
	: RenderPass("Accumulation",
		{ Width, Height },
		NumResources)
{
	
}

void Accumulation::InitializePipeline(RenderDevice* pRenderDevice)
{
	Resources[RenderTarget] = pRenderDevice->InitializeRenderResourceHandle(RenderResourceType::Texture, "Render Pass[" + Name + "]: " + "RenderTarget");

	pRenderDevice->CreateRootSignature(RootSignatures::Raytracing::Accumulation, [](RootSignatureProxy& proxy)
	{
		proxy.DenyTessellationShaderAccess();
		proxy.DenyGSAccess();
	});

	pRenderDevice->CreateComputePipelineState(ComputePSOs::Accumulation, [=](ComputePipelineStateProxy& proxy)
	{
		proxy.pRootSignature = pRenderDevice->GetRootSignature(RootSignatures::Raytracing::Accumulation);
		proxy.pCS = &Shaders::CS::Accumulation;
	});
}

void Accumulation::ScheduleResource(ResourceScheduler* pResourceScheduler, RenderGraph* pRenderGraph)
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

void Accumulation::InitializeScene(GpuScene* pGpuScene, RenderDevice* pRenderDevice)
{

}

void Accumulation::RenderGui()
{

}

void Accumulation::Execute(RenderContext& RenderContext, RenderGraph* pRenderGraph)
{
	PIXEvent(RenderContext->GetApiHandle(), L"Accumulation");

	//auto pPathtracingRenderPass = pRenderGraph->GetRenderPass<Pathtracing>();
	//Descriptor InputSRV = RenderContext.GetShaderResourceView(pPathtracingRenderPass->Resources[Pathtracing::EResources::RenderTarget]);

	//auto pAmbientOcclusionRenderPass = pRenderGraph->GetRenderPass<AmbientOcclusion>();
	//Descriptor InputSRV = RenderContext.GetShaderResourceView(pAmbientOcclusionRenderPass->Resources[AmbientOcclusion::EResources::RenderTarget]);

	auto pShadingCompositionRenderPass = pRenderGraph->GetRenderPass<ShadingComposition>();
	Descriptor InputSRV = RenderContext.GetShaderResourceView(pShadingCompositionRenderPass->Resources[ShadingComposition::EResources::RenderTarget]);

	struct RenderPassData
	{
		uint AccumulationCount;

		uint Input;
		uint RenderTarget;
	} Data;

	Data.AccumulationCount = Settings.AccumulationCount++;

	Data.Input = InputSRV.HeapIndex;
	Data.RenderTarget = RenderContext.GetUnorderedAccessView(Resources[EResources::RenderTarget]).HeapIndex;
	RenderContext.UpdateRenderPassData<RenderPassData>(Data);

	RenderContext.SetPipelineState(ComputePSOs::Accumulation);
	RenderContext->Dispatch2D(Properties.Width, Properties.Height, 8, 8);
}

void Accumulation::StateRefresh()
{
	Settings.AccumulationCount = 0;
}