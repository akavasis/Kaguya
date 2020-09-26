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
		ConstantBuffer,
		WorldPosition,
		WorldNormal,
		MaterialAlbedo,
		MaterialEmissive,
		MaterialSpecular,
		MaterialRefraction,
		MaterialExtra,
		NumResources
	};

	enum EResourceViews
	{
		GeometryTables,
		RenderTargetUAVs,
		NumResourceViews
	};

	RaytraceGBuffer(UINT Width, UINT Height);
	virtual ~RaytraceGBuffer() override;
protected:
	virtual bool Initialize(RenderDevice* pRenderDevice) override;
	virtual void InitializeScene(GpuScene* pGpuScene, RenderDevice* pRenderDevice) override;
	virtual void RenderGui() override;
	virtual void Execute(RenderGraphRegistry& RenderGraphRegistry, CommandContext* pCommandContext) override;
	virtual void Resize(UINT Width, UINT Height, RenderDevice* pRenderDevice) override;
	virtual void StateRefresh() override;
private:
	GpuScene* pGpuScene;

	RenderResourceHandle m_RayGenerationShaderTable;
	RenderResourceHandle m_MissShaderTable;
	RenderResourceHandle m_HitGroupShaderTable;
};