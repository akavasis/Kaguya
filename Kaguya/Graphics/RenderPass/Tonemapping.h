#pragma once
#include "Graphics/Scene/Scene.h"
#include "Graphics/GpuScene.h"
#include "Graphics/RenderGraph.h"
#include "Graphics/RendererRegistry.h"

class Tonemapping : public IRenderPass
{
public:
	struct SSettings
	{
		float Exposure = 0.5f;
		float Gamma = 2.2f;
	};

	Tonemapping(Descriptor Input);
	virtual ~Tonemapping() override;
protected:
	virtual bool Initialize(GpuScene* pGpuScene, RenderDevice* pRenderDevice) override;
	virtual void Update(GpuScene* pGpuScene, RenderDevice* pRenderDevice) override;
	virtual void RenderGui() override;
	virtual void Execute(RenderGraphRegistry& RenderGraphRegistry, CommandContext* pCommandContext) override;
	virtual void Resize(UINT Width, UINT Height, RenderDevice* pRenderDevice) override;
private:
	SSettings Settings;
	Descriptor Input;
	Texture* pDestination;		// Set in Renderer::Render
	Descriptor DestinationRTV;	// Set in Renderer::Render
};