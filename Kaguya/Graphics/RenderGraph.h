#pragma once
#include <memory>
#include <vector>
#include <functional>
#include <unordered_set>
#include "Core/ThreadPool.h"
#include "RenderDevice.h"
#include "Gui.h"
#include "Scene/Scene.h"
#include "GpuScene.h"
#include "RenderContext.h"

// Forward decl
class RenderPass;
class ResourceScheduler;
class ResourceRegistry;
class RenderGraph;

enum class RenderPassType
{
	Graphics,
	Compute,
	Copy
};

struct RenderTargetProperties
{
	UINT Width = 0;
	UINT Height = 0;
	DXGI_FORMAT Format = DXGI_FORMAT_UNKNOWN;
};

class RenderPass
{
public:
	inline static constexpr size_t GpuDataByteSize = 2048;

	RenderPass(RenderPassType Type, RenderTargetProperties Properties);
	virtual ~RenderPass() = default;

	void OnScheduleResource(ResourceScheduler* pResourceScheduler) { return ScheduleResource(pResourceScheduler); }
	void OnInitializeScene(GpuScene* pGpuScene, RenderDevice* pRenderDevice) { return InitializeScene(pGpuScene, pRenderDevice); }
	void OnRenderGui() { return RenderGui(); }
	void OnExecute(RenderContext& RenderContext, RenderGraph* pRenderGraph) { return Execute(RenderContext, pRenderGraph); }
	void OnStateRefresh() { Refresh = false; StateRefresh(); }

	bool Enabled;
	bool Refresh;
	RenderPassType Type;
	RenderTargetProperties Properties;
	std::vector<RenderResourceHandle> Resources;
protected:
	virtual void ScheduleResource(ResourceScheduler* pResourceScheduler) = 0;
	virtual void InitializeScene(GpuScene* pGpuScene, RenderDevice* pRenderDevice) = 0;
	virtual void RenderGui() = 0;
	virtual void Execute(RenderContext& RenderContext, RenderGraph* pRenderGraph) = 0;
	virtual void StateRefresh() = 0;
};

class ResourceScheduler
{
public:
	void AllocateBuffer(std::function<void(BufferProxy&)> Configurator)
	{
		BufferProxy proxy;
		Configurator(proxy);

		m_BufferRequests[m_pCurrentRenderPass].push_back(proxy);
	}

	void AllocateTexture(Resource::Type Type, std::function<void(TextureProxy&)> Configurator)
	{
		TextureProxy proxy(Type);
		Configurator(proxy);

		m_TextureRequests[m_pCurrentRenderPass].push_back(proxy);
	}
private:
	friend class RenderGraph;

	RenderPass* m_pCurrentRenderPass;
	std::unordered_map<RenderPass*, std::vector<BufferProxy>> m_BufferRequests;
	std::unordered_map<RenderPass*, std::vector<TextureProxy>> m_TextureRequests;
};

class RenderGraph
{
public:
	struct Settings
	{
		static constexpr bool MultiThreaded = true;
	};

	RenderGraph(RenderDevice* pRenderDevice, GpuScene* pGpuScene);

	template<typename RenderPass>
	RenderPass* GetRenderPass() const;

	void AddRenderPass(RenderPass* pIRenderPass);

	// Only call once
	void Initialize();

	// Call every frame
	void RenderGui();
	void Execute();
	void ExecuteCommandContexts(Gui* pGui);
private:
	void CreaterResources();
	void CreateResourceViews();

	RenderDevice* pRenderDevice;
	GpuScene* pGpuScene;

	ThreadPool m_ThreadPool;

	ResourceScheduler m_ResourceScheduler;
	std::vector<std::unique_ptr<RenderPass>> m_RenderPasses;
	std::vector<std::reference_wrapper<const std::type_info>> m_RenderPassIDs;
	std::vector<CommandContext*> m_CommandContexts;
	RenderResourceHandle m_GpuData;
};

#include "RenderGraph.inl"