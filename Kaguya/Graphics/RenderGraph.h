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

class ResourceScheduler
{
public:
	void AllocateTexture(Resource::Type Type, std::function<void(TextureProxy&)> Configurator);

	void Read(RenderResourceHandle Resource);

	void Write(RenderResourceHandle Resource);
private:
	friend class RenderGraph;

	RenderPass* m_pCurrentRenderPass;
	std::unordered_map<RenderPass*, std::vector<TextureProxy>> m_TextureRequests;
};

class RenderGraph
{
public:
	RenderGraph(RenderDevice* pRenderDevice);
	~RenderGraph();

	template<typename RenderPass>
	RenderPass* GetRenderPass() const;

	RenderPass* AddRenderPass(RenderPass* pRenderPass);

	// Only call once
	void Initialize(HANDLE AccelerationStructureCompleteSignal);
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
		Buffer*	pSystemConstants;
		Buffer*	pGpuData;
	};

	static DWORD WINAPI RenderPassThreadProc(_In_ PVOID pParameter);

	void CreaterResources();
	void CreateResourceViews();

	inline static bool											ExitRenderPassThread = false;

	RenderDevice*												SV_pRenderDevice;

	HANDLE														m_AccelerationStructureCompleteSignal;
	std::vector<wil::unique_event>								m_WorkerExecuteEvents;
	std::vector<wil::unique_event>								m_WorkerCompleteEvents;
	std::vector<RenderPassThreadProcParameter>					m_ThreadParameters;
	std::vector<wil::unique_handle>								m_Threads;

	ResourceScheduler											m_ResourceScheduler;
	std::vector<std::unique_ptr<RenderPass>>					m_RenderPasses;
	std::vector<CommandContext*>								m_CommandContexts;
	RenderResourceHandle										m_SystemConstants;
	RenderResourceHandle										m_GpuData;
};

#include "RenderGraph.inl"