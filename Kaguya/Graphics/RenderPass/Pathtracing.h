#pragma once
#include "Graphics/Scene/Scene.h"
#include "Graphics/GpuScene.h"
#include "Graphics/RenderGraph.h"
#include "Graphics/RendererRegistry.h"

class Pathtracing : public RenderPass
{
public:
	struct EResources
	{
		enum
		{
			ConstantBuffer,
			RenderTarget,
			NumResources
		};
	};

	struct EResourceViews
	{
		enum
		{
			GeometryTables,				
			RenderTargetUAV,
			NumResourceViews
		};
	};

	struct SSettings
	{
		int MaxDepth = 4;
	};

	Pathtracing(UINT Width, UINT Height);
	virtual ~Pathtracing() override;
protected:
	virtual bool Initialize(RenderDevice* pRenderDevice) override;
	virtual void InitializeScene(GpuScene* pGpuScene, RenderDevice* pRenderDevice) override;
	virtual void RenderGui() override;
	virtual void Execute(RenderGraphRegistry& RenderGraphRegistry, CommandContext* pCommandContext) override;
	virtual void Resize(UINT Width, UINT Height, RenderDevice* pRenderDevice) override;
	virtual void StateRefresh() override;
private:
	SSettings Settings;
	GpuScene* pGpuScene;

	RenderResourceHandle m_RayGenerationShaderTable;
	RenderResourceHandle m_MissShaderTable;
	RenderResourceHandle m_HitGroupShaderTable;
};