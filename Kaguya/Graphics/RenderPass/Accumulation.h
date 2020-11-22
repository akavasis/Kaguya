#pragma once
#include "../RenderPass.h"

class Accumulation : public RenderPass
{
public:
	enum EResources
	{
		RenderTarget,
		NumResources
	};

	struct Settings
	{
		unsigned int AccumulationCount = 0;
	};

	Accumulation(UINT Width, UINT Height);
protected:
	void InitializePipeline(RenderDevice* pRenderDevice) override;
	void ScheduleResource(ResourceScheduler* pResourceScheduler, RenderGraph* pRenderGraph) override;
	void InitializeScene(GpuScene* pGpuScene, RenderDevice* pRenderDevice) override;
	void RenderGui() override;
	void Execute(RenderContext& RenderContext, RenderGraph* pRenderGraph) override;
	void StateRefresh() override;
private:
	Settings settings;
};