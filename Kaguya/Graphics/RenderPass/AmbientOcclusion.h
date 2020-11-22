#pragma once
#include "../RenderPass.h"

class AmbientOcclusion : public RenderPass
{
public:
	enum EResources
	{
		RenderTarget,
		NumResources
	};

	struct SSettings
	{
		float	AORadius			= 1.0f;
		int		NumAORaysPerPixel	= 1;
	};

	AmbientOcclusion(UINT Width, UINT Height);
protected:
	void InitializePipeline(RenderDevice* pRenderDevice) override;
	void ScheduleResource(ResourceScheduler* pResourceScheduler, RenderGraph* pRenderGraph) override;
	void InitializeScene(GpuScene* pGpuScene, RenderDevice* pRenderDevice) override;
	void RenderGui() override;
	void Execute(RenderContext& RenderContext, RenderGraph* pRenderGraph) override;
	void StateRefresh() override;
private:
	SSettings Settings;
	GpuScene* pGpuScene;

	RenderResourceHandle m_RayGenerationShaderTable;
	RenderResourceHandle m_MissShaderTable;
	RenderResourceHandle m_HitGroupShaderTable;
};