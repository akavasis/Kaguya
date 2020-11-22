#include "pch.h"
#include "RenderPass.h"

RenderPass::RenderPass(std::string Name, RenderTargetProperties Properties)
	: Enabled(true),
	Refresh(false),
	ExplicitResourceTransition(false),
	UseRayTracing(false),
	Name(std::move(Name)),
	Properties(Properties)
{

}

void RenderPass::OnInitializePipeline(RenderDevice* pRenderDevice)
{
	return InitializePipeline(pRenderDevice);
}

void RenderPass::OnScheduleResource(ResourceScheduler* pResourceScheduler, RenderGraph* pRenderGraph)
{
	return ScheduleResource(pResourceScheduler, pRenderGraph);
}

void RenderPass::OnInitializeScene(GpuScene* pGpuScene, RenderDevice* pRenderDevice)
{
	return InitializeScene(pGpuScene, pRenderDevice);
}

void RenderPass::OnRenderGui()
{
	return RenderGui();
}

void RenderPass::OnExecute(RenderContext& RenderContext, RenderGraph* pRenderGraph)
{
	return Execute(RenderContext, pRenderGraph);
}

void RenderPass::OnStateRefresh()
{
	Refresh = false;
	StateRefresh();
}