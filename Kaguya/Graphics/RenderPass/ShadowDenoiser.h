#pragma once
#include "Graphics/Scene/Scene.h"
#include "Graphics/GpuScene.h"
#include "Graphics/RenderGraph.h"
#include "Graphics/RendererRegistry.h"

class ShadowDenoiser : public RenderPass
{
public:
	enum EResources
	{
		IntermediateNoise,
		BlurredNoise,
		Result0a,
		Result0b,
		Result1a,
		Result1b,
		WeightedShadow,
		NumResources
	};

	struct Settings
	{
		/*
			Filter radius in pixels. This will be multiplied by stepSize.
			Each 1D filter will have 2*radius + 1 taps. If set to 0, the filter is turned off.
		*/
		int Radius = 10;

		/*
			Default is to step in 2-pixel intervals. This constant can be
			increased while radius decreases to improve
			performance at the expense of some dithering artifacts.

			Must be at least 1.
		*/
		int StepSize = 1;

		/*
			If true, ensure that the "bilateral" weights are monotonically decreasing
			moving away from the current pixel.
		*/
		bool MonotonicallyDecreasingBilateralWeights = false;

		/*
			How much depth difference is taken into account.
		*/
		float DepthWeight = 1.0f;

		/*
			How much normal difference is taken into account.
		*/
		float NormalWeight = 1.5f;

		/*
			How much plane difference is taken into account.
		*/
		float PlaneWeight = 1.5f;

		/*
			How much the analytic unshadowed term is taken into account
			to avoid blurring across highlights or different shading regions.
			Because the filter is separated, making this too high can lead to
			unexpected results and axial bias.
		*/
		float AnalyticWeight = 0.09f;
	};

	ShadowDenoiser(UINT Width, UINT Height);
protected:
	void InitializePipeline(RenderDevice* pRenderDevice) override;
	void ScheduleResource(ResourceScheduler* pResourceScheduler) override;
	void InitializeScene(GpuScene* pGpuScene, RenderDevice* pRenderDevice) override;
	void RenderGui() override;
	void Execute(RenderContext& RenderContext, RenderGraph* pRenderGraph) override;
	void StateRefresh() override;
private:
	void EstimateNoise(RenderContext& RenderContext, RenderGraph* pRenderGraph);
	void FilterNoise(RenderContext& RenderContext, RenderGraph* pRenderGraph);
	void Denoise(RenderContext& RenderContext, RenderGraph* pRenderGraph);
	void WeightedShadowComposition(RenderContext& RenderContext, RenderGraph* pRenderGraph);

	Settings settings;
};