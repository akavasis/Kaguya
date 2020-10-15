#pragma once
#include "Graphics/Scene/Scene.h"
#include "Graphics/GpuScene.h"
#include "Graphics/RenderGraph.h"
#include "Graphics/RendererRegistry.h"

class RaytraceGBuffer : public RenderPass
{
public:
	enum EResources
	{
		WorldPosition,
		WorldNormal,
		MaterialAlbedo,
		MaterialEmissive,
		MaterialSpecular,
		MaterialRefraction,
		MaterialExtra,
		NumResources
	};

	struct SSettings
	{
		int GBuffer = 0;
	};

	RaytraceGBuffer(UINT Width, UINT Height);

	inline auto GetSettings() const { return Settings; }
protected:
	virtual void InitializePipeline(RenderDevice* pRenderDevice) override;
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