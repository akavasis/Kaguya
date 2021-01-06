#pragma once

#include <memory>
#include <atomic>
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
	RenderPass* GetRenderPass() const
	{
		for (size_t i = 0; i < m_RenderPasses.size(); ++i)
		{
			if (typeid(*m_RenderPasses[i].get()) == typeid(RenderPass))
			{
				return static_cast<RenderPass*>(m_RenderPasses[i].get());
			}
		}

		return nullptr;
	}

	RenderPass* AddRenderPass(RenderPass* pRenderPass);

	UINT NumRenderPass() const { return m_RenderPasses.size(); }

	// Only call once
	void Initialize(HANDLE AccelerationStructureCompleteSignal);
	void InitializeScene(GpuScene* pGpuScene);

	// Call every frame
	void RenderGui();
	void UpdateSystemConstants(const HLSL::SystemConstants& HostSystemConstants);
	void Execute(bool Refresh);

	// Returns the command contexts that needs to be executed
	std::vector<CommandContext*>& GetCommandContexts();
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

	inline static std::atomic<bool>								ExitRenderPassThread = false;

	RenderDevice*												SV_pRenderDevice;

	HANDLE														m_AccelerationStructureCompleteSignal = nullptr;
	std::vector<CommandContext*>								m_CommandContexts;
	std::vector<wil::unique_event>								m_WorkerExecuteEvents;
	std::vector<wil::unique_event>								m_WorkerCompleteEvents;
	std::vector<RenderPassThreadProcParameter>					m_ThreadParameters;
	std::vector<wil::unique_handle>								m_Threads;

	ResourceScheduler											m_ResourceScheduler;
	std::vector<std::unique_ptr<RenderPass>>					m_RenderPasses;
	RenderResourceHandle										m_SystemConstants;
	RenderResourceHandle										m_GpuData;
};