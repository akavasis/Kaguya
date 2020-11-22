#pragma once
#include "../RenderPass.h"

class GBuffer : public RenderPass
{
public:
	enum EResources
	{
		Albedo,
		Normal,
		TypeAndIndex,
		SVGF_LinearZ,
		SVGF_MotionVector,
		SVGF_Compact,

		Depth,

		NumResources
	};

	GBuffer(UINT Width, UINT Height);
protected:
	void InitializePipeline(RenderDevice* pRenderDevice) override;
	void ScheduleResource(ResourceScheduler* pResourceScheduler, RenderGraph* pRenderGraph) override;
	void InitializeScene(GpuScene* pGpuScene, RenderDevice* pRenderDevice) override;
	void RenderGui() override;
	void Execute(RenderContext& RenderContext, RenderGraph* pRenderGraph) override;
	void StateRefresh() override;
private:
	void RenderMeshes(RenderContext& RenderContext);
	void RenderLights(RenderContext& RenderContext);

	GpuScene* pGpuScene;
};