#pragma once
#include <memory>
#include <vector>
#include <unordered_set>
#include <string>
#include "RenderDevice.h"
#include "Scene/Scene.h"
#include "GpuScene.h"
#include "RenderContext.h"

// Forward decl
class RenderPass;
class ResourceScheduler;
class ResourceRegistry;
class RenderGraph;

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

	RenderPass(std::string Name, RenderTargetProperties Properties);
	virtual ~RenderPass() = default;

	void OnInitializePipeline(RenderDevice* pRenderDevice) { return InitializePipeline(pRenderDevice); }
	void OnScheduleResource(ResourceScheduler* pResourceScheduler, RenderGraph* pRenderGraph) { return ScheduleResource(pResourceScheduler, pRenderGraph); }
	void OnInitializeScene(GpuScene* pGpuScene, RenderDevice* pRenderDevice) { return InitializeScene(pGpuScene, pRenderDevice); }
	void OnRenderGui() { return RenderGui(); }
	void OnExecute(RenderContext& RenderContext, RenderGraph* pRenderGraph) { return Execute(RenderContext, pRenderGraph); }
	void OnStateRefresh() { Refresh = false; StateRefresh(); }

	bool Enabled;
	bool Refresh;
	bool ExplicitResourceTransition; // If this is true, then the render pass is responsible for handling resource transitions, by default, this is false
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

	std::vector<RenderResourceHandle> Reads;
	std::vector<RenderResourceHandle> Writes;
};

class ResourceScheduler
{
public:
	void AllocateTexture(DeviceResource::Type Type, std::function<void(DeviceTextureProxy&)> Configurator)
	{
		DeviceTextureProxy proxy(Type);
		Configurator(proxy);

		m_TextureRequests[m_pCurrentRenderPass].push_back(proxy);
	}

	void Read(RenderResourceHandle Resource)
	{
		assert(Resource.Type == RenderResourceType::DeviceBuffer || Resource.Type == RenderResourceType::DeviceTexture);

		m_pCurrentRenderPass->Reads.push_back(Resource);
	}

	void Write(RenderResourceHandle Resource)
	{
		assert(Resource.Type == RenderResourceType::DeviceBuffer || Resource.Type == RenderResourceType::DeviceTexture);

		m_pCurrentRenderPass->Writes.push_back(Resource);
	}
private:
	friend class RenderGraph;

	RenderPass* m_pCurrentRenderPass;
	std::unordered_map<RenderPass*, std::vector<DeviceTextureProxy>> m_TextureRequests;
};

class RenderGraph
{
public:
	RenderGraph(RenderDevice* pRenderDevice);
	~RenderGraph();

	template<typename RenderPass>
	RenderPass* GetRenderPass() const;

	void AddRenderPass(RenderPass* pRenderPass);

	// Only call once
	void Initialize();
	void InitializeScene(GpuScene* pGpuScene);

	// Call every frame
	void RenderGui();
	void UpdateSystemConstants(const HLSL::SystemConstants& HostSystemConstants);
	void Execute(bool Refresh);
	void ExecuteCommandContexts(RenderContext& RendererRenderContext);
private:
	struct RenderPassThreadProcParameter
	{
		RenderGraph*	pRenderGraph;
		int32_t			ThreadID;
		HANDLE			ExecuteSignal;
		HANDLE			CompleteSignal;
		RenderPass*		pRenderPass;
		RenderDevice*	pRenderDevice;
		CommandContext* pCommandContext;
		DeviceBuffer*	pSystemConstants;
		DeviceBuffer*	pGpuData;
	};

	static DWORD WINAPI RenderPassThreadProc(_In_ PVOID pParameter);

	void CreaterResources();
	void CreateResourceViews();

	inline static bool											ExitRenderPassThread = false;

	RenderDevice*												SV_pRenderDevice;

	std::vector<HANDLE>											m_WorkerExecuteSignals;
	std::vector<HANDLE>											m_WorkerCompleteSignals;
	std::vector<RenderPassThreadProcParameter>					m_ThreadParameters;
	std::vector<HANDLE>											m_Threads;

	ResourceScheduler											m_ResourceScheduler;
	std::vector<std::unique_ptr<RenderPass>>					m_RenderPasses;
	std::vector<std::reference_wrapper<const std::type_info>>	m_RenderPassIDs;
	std::vector<CommandContext*>								m_CommandContexts;
	RenderResourceHandle										m_SystemConstants;
	RenderResourceHandle										m_GpuData;
};

#include "RenderGraph.inl"