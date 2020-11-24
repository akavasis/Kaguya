#pragma once
#include "../RenderPass.h"

//----------------------------------------------------------------------------------------------------
struct SVGFSettings
{
	inline static float Alpha			= 0.05f;
	inline static float MomentsAlpha	= 0.2f;
	inline static float PhiColor		= 0.25f;
	inline static float PhiNormal		= 128.0f;
};

//----------------------------------------------------------------------------------------------------
class SVGFReproject : public RenderPass
{
public:
	enum EResources
	{
		PrevRenderTarget0,
		PrevRenderTarget1,
		PrevMoments,
		PrevLinearZ,
		PrevHistoryLength,

		RenderTarget0,
		RenderTarget1,
		Moments,
		HistoryLength,
		NumResources
	};

	SVGFReproject(UINT Width, UINT Height);
protected:
	void InitializePipeline(RenderDevice* pRenderDevice) override;
	void ScheduleResource(ResourceScheduler* pResourceScheduler, RenderGraph* pRenderGraph) override;
	void InitializeScene(GpuScene* pGpuScene, RenderDevice* pRenderDevice) override;
	void RenderGui() override;
	void Execute(RenderContext& RenderContext, RenderGraph* pRenderGraph) override;
	void StateRefresh() override;
private:
};

//----------------------------------------------------------------------------------------------------
class SVGFFilterMoments : public RenderPass
{
public:
	enum EResources
	{
		RenderTarget0,
		RenderTarget1,
		NumResources
	};

	SVGFFilterMoments(UINT Width, UINT Height);
protected:
	void InitializePipeline(RenderDevice* pRenderDevice) override;
	void ScheduleResource(ResourceScheduler* pResourceScheduler, RenderGraph* pRenderGraph) override;
	void InitializeScene(GpuScene* pGpuScene, RenderDevice* pRenderDevice) override;
	void RenderGui() override;
	void Execute(RenderContext& RenderContext, RenderGraph* pRenderGraph) override;
	void StateRefresh() override;
private:
};

//----------------------------------------------------------------------------------------------------
class SVGFAtrous : public RenderPass
{
public:
	enum EResources
	{
		RenderTarget0,
		RenderTarget1,
		NumResources
	};

	SVGFAtrous(UINT Width, UINT Height);
protected:
	void InitializePipeline(RenderDevice* pRenderDevice) override;
	void ScheduleResource(ResourceScheduler* pResourceScheduler, RenderGraph* pRenderGraph) override;
	void InitializeScene(GpuScene* pGpuScene, RenderDevice* pRenderDevice) override;
	void RenderGui() override;
	void Execute(RenderContext& RenderContext, RenderGraph* pRenderGraph) override;
	void StateRefresh() override;
private:
};