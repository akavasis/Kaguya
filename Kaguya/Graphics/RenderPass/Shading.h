#pragma once
#include "Graphics/Scene/Scene.h"
#include "Graphics/GpuScene.h"
#include "Graphics/RenderGraph.h"
#include "Graphics/RendererRegistry.h"

class Shading : public RenderPass
{
public:
	enum EResources
	{
		RenderTarget,
		NumResources
	};

	Shading(UINT Width, UINT Height);
protected:
	virtual void InitializePipeline(RenderDevice* pRenderDevice) override;
	virtual void ScheduleResource(ResourceScheduler* pResourceScheduler) override;
	virtual void InitializeScene(GpuScene* pGpuScene, RenderDevice* pRenderDevice) override;
	virtual void RenderGui() override;
	virtual void Execute(RenderContext& RenderContext, RenderGraph* pRenderGraph) override;
	virtual void StateRefresh() override;
private:
	GpuScene* pGpuScene;

	RenderResourceHandle m_RayGenerationShaderTable;
};