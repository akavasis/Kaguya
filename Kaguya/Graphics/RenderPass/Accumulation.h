#pragma once
#include "Graphics/Scene/Scene.h"
#include "Graphics/GpuScene.h"
#include "Graphics/RenderGraph.h"
#include "Graphics/RendererRegistry.h"

class Accumulation : public RenderPass
{
public:
	struct EResources
	{
		enum
		{
			RenderTarget,
			NumResources
		};
	};

	struct EResourceViews
	{
		enum
		{
			RenderTargetUAV,
			RenderTargetSRV,
			NumResourceViews
		};
	};

	struct SSettings
	{
		unsigned int AccumulationCount = 0;
	};

	Accumulation(UINT Width, UINT Height);
	virtual ~Accumulation() override;
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

	float LastAperture;
	float LastFocalLength;
	Transform LastCameraTransform;
};