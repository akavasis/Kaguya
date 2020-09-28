#pragma once
#include "Graphics/Scene/Scene.h"
#include "Graphics/GpuScene.h"
#include "Graphics/RenderGraph.h"
#include "Graphics/RendererRegistry.h"

class Pathtracing : public RenderPass
{
public:
	enum EResources
	{
		ConstantBuffer,
		RenderTarget,
		NumResources
	};

	struct SSettings
	{
		int MaxDepth = 4;
	};

	Pathtracing(UINT Width, UINT Height);
	virtual ~Pathtracing() override;
protected:
	virtual void ScheduleResource(ResourceScheduler* pResourceScheduler) override;
	virtual void InitializeScene(GpuScene* pGpuScene, RenderDevice* pRenderDevice) override;
	virtual void RenderGui() override;
	virtual void Execute(ResourceRegistry& ResourceRegistry, CommandContext* pCommandContext) override;
	virtual void StateRefresh() override;
private:
	SSettings Settings;
	GpuScene* pGpuScene;

	RenderResourceHandle m_RayGenerationShaderTable;
	RenderResourceHandle m_MissShaderTable;
	RenderResourceHandle m_HitGroupShaderTable;
};