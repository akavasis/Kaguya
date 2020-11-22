#pragma once
#include "../RenderPass.h"

class PostProcess : public RenderPass
{
public:
	enum EResources
	{
		RenderTarget,

		// a/b is used to ping pong
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

	struct Settings
	{
		bool			ApplyBloom			= true;
		struct Bloom
		{
			float		Threshold			= 4.0f;
			float		Intensity			= 0.1f;
		} Bloom;
	};

	struct GranTurismoOperator
	{
		float			MaximumBrightness	= 1.0f;
		float			Contrast			= 1.0f;
		float			LinearSectionStart	= 0.22f;
		float			LinearSectionLength = 0.4f;
		float			BlackTightness_C	= 1.33f;
		float			BlackTightness_B	= 0.0f;
	};

	PostProcess(UINT Width, UINT Height);
protected:
	void InitializePipeline(RenderDevice* pRenderDevice) override;
	void ScheduleResource(ResourceScheduler* pResourceScheduler, RenderGraph* pRenderGraph) override;
	void InitializeScene(GpuScene* pGpuScene, RenderDevice* pRenderDevice) override;
	void RenderGui() override;
	void Execute(RenderContext& RenderContext, RenderGraph* pRenderGraph) override;
	void StateRefresh() override;
private:
	void ApplyBloom(Descriptor InputSRV, RenderContext& RenderContext, RenderGraph* pRenderGraph);
	void ApplyTonemapAndGuiToSwapChain(Descriptor InputSRV, RenderContext& RenderContext, RenderGraph* pRenderGraph);

	void Blur(size_t Input, size_t Output, RenderContext& RenderContext);
	void UpsampleBlurAccumulation(size_t HighResolution, size_t LowResolution, size_t Output, RenderContext& RenderContext);

	Settings settings;
	GranTurismoOperator granTurismoOperator;
};