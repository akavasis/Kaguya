#pragma once
#include "Graphics/Scene/Scene.h"
#include "Graphics/GpuScene.h"
#include "Graphics/RenderGraph.h"
#include "Graphics/RendererRegistry.h"

class Accumulation : public IRenderPass
{
public:
	struct EResources
	{
		enum
		{
			RenderTarget
		};
	};

	struct EResourceViews
	{
		enum
		{
			RenderTargetUnorderedAccess,
			RenderTargetShaderResource
		};
	};

	struct SSettings
	{
		unsigned int AccumulationCount = 0;
	};

	Accumulation(UINT Width, UINT Height);
	virtual ~Accumulation() override;
protected:
	virtual bool Initialize(GpuScene* pGpuScene, RenderDevice* pRenderDevice) override;
	virtual void Update(GpuScene* pGpuScene, RenderDevice* pRenderDevice) override;
	virtual void RenderGui() override;
	virtual void Execute(RenderGraphRegistry& RenderGraphRegistry, CommandContext* pCommandContext) override;
	virtual void Resize(UINT Width, UINT Height, RenderDevice* pRenderDevice) override;
private:
	SSettings Settings; // Set in Renderer
	float LastAperture;
	float LastFocalLength;
	Transform LastCameraTransform;
};