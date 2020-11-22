#pragma once
#include "../RenderPass.h"

class Shading : public RenderPass
{
public:
	enum EResources
	{
		AnalyticUnshadowed,
		StochasticUnshadowed,
		StochasticShadowed,
		NumResources
	};

	Shading(UINT Width, UINT Height);
protected:
	void InitializePipeline(RenderDevice* pRenderDevice) override;
	void ScheduleResource(ResourceScheduler* pResourceScheduler, RenderGraph* pRenderGraph) override;
	void InitializeScene(GpuScene* pGpuScene, RenderDevice* pRenderDevice) override;
	void RenderGui() override;
	void Execute(RenderContext& RenderContext, RenderGraph* pRenderGraph) override;
	void StateRefresh() override;
private:
	GpuScene* pGpuScene;

	RenderResourceHandle m_RayGenerationShaderTable;
	RenderResourceHandle m_MissShaderTable;
	RenderResourceHandle m_HitGroupShaderTable;
};