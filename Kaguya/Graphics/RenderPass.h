#pragma once
#include "RenderDevice.h"

class ResourceScheduler;
class RenderGraph;
class GpuScene;
class RenderContext;

struct RenderTargetProperties
{
	UINT Width = 0;
	UINT Height = 0;
};

class RenderPass
{
public:
	inline static constexpr size_t GpuDataByteSize = 2048;

	RenderPass(std::string Name, RenderTargetProperties Properties, UINT NumResources);
	virtual ~RenderPass() = default;

	void OnInitializePipeline(RenderDevice* pRenderDevice);
	void OnScheduleResource(ResourceScheduler* pResourceScheduler, RenderGraph* pRenderGraph);
	void OnInitializeScene(GpuScene* pGpuScene, RenderDevice* pRenderDevice);
	void OnRenderGui();
	void OnExecute(RenderContext& RenderContext, RenderGraph* pRenderGraph);
	void OnStateRefresh();

	UINT SV_Index;						// Given by the render graph

	std::atomic<bool> Enabled;
	std::atomic<bool> Refresh;
	std::atomic<bool> ExplicitResourceTransition;	// If this is true, then the render pass is responsible for handling resource transitions, by default, this is false
	std::atomic<bool> UseRayTracing;				// If this is true, then the render pass will wait until async compute is done generating the acceleration structure
	std::string Name;
	RenderTargetProperties Properties;
	std::vector<RenderResourceHandle> Resources;
protected:
	virtual void InitializePipeline(RenderDevice* pRenderDevice) = 0;
	virtual void ScheduleResource(ResourceScheduler* pResourceScheduler, RenderGraph* pRenderGraph) = 0;
	virtual void InitializeScene(GpuScene* pGpuScene, RenderDevice* pRenderDevice) = 0;
	virtual void RenderGui() = 0;
	virtual void Execute(RenderContext& RenderContext, RenderGraph* pRenderGraph) = 0;
	virtual void StateRefresh() = 0;
private:
	friend class RenderGraph;
	friend class ResourceScheduler;

	std::unordered_set<RenderResourceHandle> Reads;
	std::unordered_set<RenderResourceHandle> Writes;
};