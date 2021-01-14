#include "pch.h"
#include "Accumulation.h"

#include "Graphics/Scene/Scene.h"
#include "Graphics/GpuScene.h"
#include "Graphics/RenderGraph.h"
#include "Graphics/RendererRegistry.h"

#include "Pathtracing.h"

Accumulation::Accumulation(UINT Width, UINT Height)
	: RenderPass("Accumulation",
		{ Width, Height },
		NumResources)
{
	
}

void Accumulation::InitializePipeline(RenderDevice* pRenderDevice)
{
	Resources[RenderTarget] = pRenderDevice->InitializeRenderResourceHandle(RenderResourceType::Texture, "Render Pass[" + Name + "]: " + "RenderTarget");

	pRenderDevice->CreateComputePipelineState(ComputePSOs::Accumulation, [=](ComputePipelineStateBuilder& Builder)
	{
		Builder.pRootSignature = pRenderDevice->GetRootSignature(RootSignatures::Default);
		Builder.pCS = &Shaders::CS::Accumulation;
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
	if (ImGui::TreeNode(Name.data()))
	{
		ImGui::Text("Samples Accumulated: %u", Settings.AccumulationCount);

		ImGui::TreePop();
	}
}

void Accumulation::Execute(RenderContext& RenderContext, RenderGraph* pRenderGraph)
{
	auto pPathtracingRenderPass = pRenderGraph->GetRenderPass<Pathtracing>();
	Descriptor InputSRV = RenderContext.GetShaderResourceView(pPathtracingRenderPass->Resources[Pathtracing::EResources::RenderTarget]);

	struct RenderPassData
	{
		uint AccumulationCount;

		uint Input;
		uint RenderTarget;
	} g_RenderPassData = {};

	g_RenderPassData.AccumulationCount = Settings.AccumulationCount++;

	g_RenderPassData.Input = InputSRV.HeapIndex;
	g_RenderPassData.RenderTarget = RenderContext.GetUnorderedAccessView(Resources[EResources::RenderTarget]).HeapIndex;
	
	RenderContext.UpdateRenderPassData<RenderPassData>(g_RenderPassData);

	RenderContext.SetPipelineState(ComputePSOs::Accumulation);
	RenderContext.GetCommandList().Dispatch2D<8, 8>(Properties.Width, Properties.Height);
}

void Accumulation::StateRefresh()
{
	Settings.AccumulationCount = 0;
}