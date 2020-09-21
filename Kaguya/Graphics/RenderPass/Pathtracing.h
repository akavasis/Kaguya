#pragma once
#include "Graphics/Scene/Scene.h"
#include "Graphics/GpuScene.h"
#include "Graphics/RenderGraph.h"
#include "Graphics/RendererRegistry.h"

class Pathtracing : public IRenderPass
{
public:
	struct EResources
	{
		enum
		{
			ConstantBuffer,	// Render pass data
			RenderTarget	// Output render target
		};
	};

	struct EResourceViews
	{
		enum
		{
			GeometryTables,				// Shader resource views for material, vertex, index, and geometry info buffers
			RenderTargetUnorderedAccess	// Output UAV for Raytracing
		};
	};

	Pathtracing(UINT Width, UINT Height);
	virtual ~Pathtracing() override;
protected:
	virtual bool Initialize(GpuScene* pGpuScene, RenderDevice* pRenderDevice) override;
	virtual void Update(GpuScene* pGpuScene, RenderDevice* pRenderDevice) override;
	virtual void RenderGui() override;
	virtual void Execute(RenderGraphRegistry& RenderGraphRegistry, CommandContext* pCommandContext) override;
	virtual void Resize(UINT Width, UINT Height, RenderDevice* pRenderDevice) override;
private:
	RenderResourceHandle m_RayGenerationShaderTable;
	RenderResourceHandle m_MissShaderTable;
	RenderResourceHandle m_HitGroupShaderTable;
};