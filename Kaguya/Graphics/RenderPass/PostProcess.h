#pragma once
#include "Graphics/Scene/Scene.h"
#include "Graphics/GpuScene.h"
#include "Graphics/RenderGraph.h"
#include "Graphics/RendererRegistry.h"

class PostProcess : public RenderPass
{
public:
	enum EResources
	{
		RenderTarget,
		BloomRenderTarget1a,	// 1
		BloomRenderTarget1b,	// 1
		BloomRenderTarget2a,	// 1/2
		BloomRenderTarget2b,	// 1/2
		BloomRenderTarget3a,	// 1/4
		BloomRenderTarget3b,	// 1/4
		BloomRenderTarget4a,	// 1/8
		BloomRenderTarget4b,	// 1/8
		BloomRenderTarget5a,	// 1/16
		BloomRenderTarget5b,	// 1/16
		NumResources
	};

	struct SSettings
	{
		bool ApplyBloom = true;
		struct Bloom
		{
			float Threshold = 4.0f;
			float Intensity = 0.03f;
		} Bloom;
		struct Tonemapping
		{
			float Exposure = 0.5f;
			float Gamma = 2.2f;
		} Tonemapping;
	};

	PostProcess(UINT Width, UINT Height);
	virtual ~PostProcess() override;
protected:
	virtual void ScheduleResource(ResourceScheduler* pResourceScheduler) override;
	virtual void InitializeScene(GpuScene* pGpuScene, RenderDevice* pRenderDevice) override;
	virtual void RenderGui() override;
	virtual void Execute(ResourceRegistry& ResourceRegistry, CommandContext* pCommandContext) override;
	virtual void StateRefresh() override;
private:
	void Blur(size_t Input, size_t Output, ResourceRegistry& ResourceRegistry, CommandContext* pCommandContext);
	void UpsampleBlur(size_t HighResolution, size_t LowResolution, size_t Output, ResourceRegistry& ResourceRegistry, CommandContext* pCommandContext);
	void ApplyBloom(ResourceRegistry& ResourceRegistry, CommandContext* pCommandContext);
	void ApplyTonemappingToSwapChain(ResourceRegistry& ResourceRegistry, CommandContext* pCommandContext);

	SSettings Settings;
};