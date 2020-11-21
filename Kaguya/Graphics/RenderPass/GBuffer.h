#pragma once
#include "Graphics/Scene/Scene.h"
#include "Graphics/GpuScene.h"
#include "Graphics/RenderGraph.h"
#include "Graphics/RendererRegistry.h"

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

	struct SSettings
	{
		int GBuffer = 0;
	};

	GBuffer(UINT Width, UINT Height);

	inline auto GetSettings() const { return Settings; }
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

	SSettings Settings;
	GpuScene* pGpuScene;
};