#pragma once
#include "Graphics/Scene/Scene.h"
#include "Graphics/GpuScene.h"
#include "Graphics/RenderGraph.h"
#include "Graphics/RendererRegistry.h"

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
	virtual void ScheduleResource(ResourceScheduler* pResourceScheduler) override;
	virtual void InitializeScene(GpuScene* pGpuScene, RenderDevice* pRenderDevice) override;
	virtual void RenderGui() override;
	virtual void Execute(RenderContext& RenderContext, RenderGraph* pRenderGraph) override;
	virtual void StateRefresh() override;
private:
	SSettings Settings;
	GpuScene* pGpuScene;

	RenderResourceHandle m_RayGenerationShaderTable;
	RenderResourceHandle m_MissShaderTable;
	RenderResourceHandle m_HitGroupShaderTable;
};