#pragma once
#include "Graphics/Scene/Scene.h"
#include "Graphics/GpuScene.h"
#include "Graphics/RenderGraph.h"
#include "Graphics/RendererRegistry.h"

class Accumulation : public IRenderPass
{
public:
	struct EOutputs
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
			RenderTarget
		};
	};

	struct SSettings
	{
		unsigned int AccumulationCount = 0;
	};

	Accumulation(UINT Width, UINT Height);
	virtual ~Accumulation() override;

	virtual void Setup(RenderDevice* pRenderDevice) override;
	virtual void Update() override;
	virtual void RenderGui() override;
	virtual void Execute(const Scene& Scene, RenderGraphRegistry& RenderGraphRegistry, CommandContext* pCommandContext) override;
	virtual void Resize(UINT Width, UINT Height, RenderDevice* pRenderDevice) override;

	SSettings Settings; // Set in Renderer
	float LastAperture;
	float LastFocalLength;
	Transform LastCameraTransform;
};