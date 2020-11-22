#pragma once
#include "../RenderPass.h"

class Pathtracing : public RenderPass
{
public:
	enum EResources
	{
		RenderTarget,
		NumResources
	};

	struct Settings
	{
		int NumSamplesPerPixel	= 1;
		int MaxDepth			= 4;
	};

	Pathtracing(UINT Width, UINT Height);
protected:
	void InitializePipeline(RenderDevice* pRenderDevice) override;
	void ScheduleResource(ResourceScheduler* pResourceScheduler, RenderGraph* pRenderGraph) override;
	void InitializeScene(GpuScene* pGpuScene, RenderDevice* pRenderDevice) override;
	void RenderGui() override;
	void Execute(RenderContext& RenderContext, RenderGraph* pRenderGraph) override;
	void StateRefresh() override;
private:
	Settings settings;
	GpuScene* pGpuScene;

	RenderResourceHandle m_RayGenerationShaderTable;
	RenderResourceHandle m_MissShaderTable;
	RenderResourceHandle m_HitGroupShaderTable;
};