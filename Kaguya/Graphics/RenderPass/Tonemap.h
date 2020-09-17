#pragma once
#include "Graphics/Scene/Scene.h"
#include "Graphics/GpuScene.h"
#include "Graphics/RenderGraph.h"
#include "Graphics/RendererRegistry.h"

class Tonemap : public IRenderPass
{
public:
	struct SSettings
	{
		float Exposure = 0.5f;
		float Gamma = 2.2f;
		unsigned int InputMapIndex = 0;
	};

	Tonemap();
	virtual ~Tonemap() override;

	virtual void Setup(RenderDevice* pRenderDevice) override;
	virtual void Update() override;
	virtual void RenderGui() override;
	virtual void Execute(const Scene& Scene, RenderGraphRegistry& RenderGraphRegistry, CommandContext* pCommandContext) override;
	virtual void Resize(UINT Width, UINT Height, RenderDevice* pRenderDevice) override;

	SSettings Settings;
	Texture* pDestination;		// Set in Renderer::Render
	Descriptor DestinationRTV;	// Set in Renderer::Render
};