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
	RenderPass(RenderPassType Type, RenderTargetProperties Properties);
	virtual ~RenderPass() = default;

	void OnScheduleResource(ResourceScheduler* pResourceScheduler) { return ScheduleResource(pResourceScheduler); }
	void OnInitializeScene(GpuScene* pGpuScene, RenderDevice* pRenderDevice) { return InitializeScene(pGpuScene, pRenderDevice); }
	void OnRenderGui() { return RenderGui(); }
	void OnExecute(ResourceRegistry& ResourceRegistry, CommandContext* pCommandContext) { return Execute(ResourceRegistry, pCommandContext); }
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
	virtual void Execute(ResourceRegistry& ResourceRegistry, CommandContext* pCommandContext) = 0;
	virtual void StateRefresh() = 0;
};

class ResourceScheduler
{
public:
	void AllocateBuffer(Delegate<void(BufferProxy&)> Configurator)
	{
		if (!m_pCurrentRenderPass)
			return;

		BufferProxy proxy;
		Configurator(proxy);

		m_BufferRequests[m_pCurrentRenderPass].push_back(proxy);
	}

	void AllocateTexture(Resource::Type Type, Delegate<void(TextureProxy&)> Configurator)
	{
		if (!m_pCurrentRenderPass)
			return;

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

class ResourceRegistry
{
public:
	ResourceRegistry(RenderDevice* pRenderDevice, RenderGraph* pRenderGraph)
		: pRenderDevice(pRenderDevice),
		pRenderGraph(pRenderGraph)
	{
	}

	[[nodiscard]] inline Buffer* GetBuffer(RenderResourceHandle Handle) const { return pRenderDevice->GetBuffer(Handle); }
	[[nodiscard]] inline Texture* GetTexture(RenderResourceHandle Handle) const { return pRenderDevice->GetTexture(Handle); }
	[[nodiscard]] inline RootSignature* GetRootSignature(RenderResourceHandle RenderResourceHandle) const { return pRenderDevice->GetRootSignature(RenderResourceHandle); }
	[[nodiscard]] inline GraphicsPipelineState* GetGraphicsPSO(RenderResourceHandle RenderResourceHandle) const { return pRenderDevice->GetGraphicsPSO(RenderResourceHandle); }
	[[nodiscard]] inline ComputePipelineState* GetComputePSO(RenderResourceHandle RenderResourceHandle) const { return pRenderDevice->GetComputePSO(RenderResourceHandle); }
	[[nodiscard]] inline RaytracingPipelineState* GetRaytracingPSO(RenderResourceHandle RenderResourceHandle) const { return pRenderDevice->GetRaytracingPSO(RenderResourceHandle); }

	[[nodiscard]] size_t GetShaderResourceDescriptorIndex(RenderResourceHandle Handle) const
	{
		if (auto iter = m_ShaderResourceTable.find(Handle);
			iter != m_ShaderResourceTable.end())
			return iter->second;
		return -1;
	}

	[[nodiscard]] size_t GetUnorderedAccessDescriptorIndex(RenderResourceHandle Handle) const
	{
		if (auto iter = m_UnorderedAccessTable.find(Handle);
			iter != m_UnorderedAccessTable.end())
			return iter->second;
		return -1;
	}

	[[nodiscard]] inline auto GetUniversalGpuDescriptorHeapSRVDescriptorHandleFromStart() const { return pRenderDevice->GetUniversalGpuDescriptorHeapSRVDescriptorHandleFromStart(); }
	[[nodiscard]] inline auto GetUniversalGpuDescriptorHeapUAVDescriptorHandleFromStart() const { return pRenderDevice->GetUniversalGpuDescriptorHeapUAVDescriptorHandleFromStart(); }

	[[nodiscard]] inline auto GetCurrentSwapChainBuffer() const { return pRenderDevice->GetTexture(pRenderDevice->SwapChainTextures[pRenderDevice->FrameIndex]); }
	[[nodiscard]] inline auto GetCurrentSwapChainRTV() const { return pRenderDevice->SwapChainRenderTargetViews[pRenderDevice->FrameIndex]; }

	template<typename RenderPass>
	RenderPass* GetRenderPass() const { return pRenderGraph->GetRenderPass<RenderPass>(); }

	DescriptorAllocation ShaderResourceViews;
	DescriptorAllocation UnorderedAccessViews;
private:
	friend class RenderGraph;

	RenderDevice* pRenderDevice;
	RenderGraph* pRenderGraph;
	std::unordered_map<RenderResourceHandle, size_t> m_ShaderResourceTable;
	std::unordered_map<RenderResourceHandle, size_t> m_UnorderedAccessTable;
};

class RenderGraph
{
public:
	struct Settings
	{
		static constexpr bool MultiThreaded = false;
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
	ResourceRegistry m_ResourceRegistry;
	std::vector<std::unique_ptr<RenderPass>> m_RenderPasses;
	std::vector<std::reference_wrapper<const std::type_info>> m_RenderPassIDs;
	std::vector<CommandContext*> m_CommandContexts;
};

#include "RenderGraph.inl"