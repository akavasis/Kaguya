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
class RenderGraphRegistry;
class RenderPass;
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

	bool OnInitialize(RenderDevice* pRenderDevice) { return Initialize(pRenderDevice); }
	void OnInitializeScene(GpuScene* pGpuScene, RenderDevice* pRenderDevice) { return InitializeScene(pGpuScene, pRenderDevice); }
	void OnRenderGui() { return RenderGui(); }
	void OnExecute(RenderGraphRegistry& RenderGraphRegistry, CommandContext* pCommandContext) { return Execute(RenderGraphRegistry, pCommandContext); }
	void OnResize(UINT Width, UINT Height, RenderDevice* pRenderDevice) { return Resize(Width, Height, pRenderDevice); }
	void OnStateRefresh() { Refresh = false; StateRefresh(); }

	bool Enabled;
	bool Refresh;
	RenderPassType Type;
	RenderTargetProperties Properties;
	std::vector<RenderResourceHandle> Resources;
	std::vector<DescriptorAllocation> ResourceViews;
protected:
	virtual bool Initialize(RenderDevice* pRenderDevice) = 0;
	virtual void InitializeScene(GpuScene* pGpuScene, RenderDevice* pRenderDevice) = 0;
	virtual void RenderGui() = 0;
	virtual void Execute(RenderGraphRegistry& RenderGraphRegistry, CommandContext* pCommandContext) = 0;
	virtual void Resize(UINT Width, UINT Height, RenderDevice* pRenderDevice)
	{
		Properties.Width = Width;
		Properties.Height = Height;
	}
	virtual void StateRefresh() = 0;
};

// TODO: Add a RenderGraphScheduler to indicate reads/writes to a particular resource so we can
// apply resource barriers ahead of time, removing the need to place them in code
class RenderGraphRegistry
{
public:
	RenderGraphRegistry(RenderDevice* pRenderDevice, RenderGraph* pRenderGraph)
		: pRenderDevice(pRenderDevice),
		pRenderGraph(pRenderGraph)
	{}

	[[nodiscard]] inline Buffer* GetBuffer(const RenderResourceHandle& RenderResourceHandle) const { return pRenderDevice->GetBuffer(RenderResourceHandle); }
	[[nodiscard]] inline Texture* GetTexture(const RenderResourceHandle& RenderResourceHandle) const { return pRenderDevice->GetTexture(RenderResourceHandle); }
	[[nodiscard]] inline Heap* GetHeap(const RenderResourceHandle& RenderResourceHandle) const { return pRenderDevice->GetHeap(RenderResourceHandle); }
	[[nodiscard]] inline RootSignature* GetRootSignature(const RenderResourceHandle& RenderResourceHandle) const { return pRenderDevice->GetRootSignature(RenderResourceHandle); }
	[[nodiscard]] inline GraphicsPipelineState* GetGraphicsPSO(const RenderResourceHandle& RenderResourceHandle) const { return pRenderDevice->GetGraphicsPSO(RenderResourceHandle); }
	[[nodiscard]] inline ComputePipelineState* GetComputePSO(const RenderResourceHandle& RenderResourceHandle) const { return pRenderDevice->GetComputePSO(RenderResourceHandle); }
	[[nodiscard]] inline RaytracingPipelineState* GetRaytracingPSO(const RenderResourceHandle& RenderResourceHandle) const { return pRenderDevice->GetRaytracingPSO(RenderResourceHandle); }

	[[nodiscard]] inline auto GetUniversalGpuDescriptorHeapSRVDescriptorHandleFromStart() const { return pRenderDevice->GetUniversalGpuDescriptorHeapSRVDescriptorHandleFromStart(); }
	[[nodiscard]] inline auto GetUniversalGpuDescriptorHeapUAVDescriptorHandleFromStart() const { return pRenderDevice->GetUniversalGpuDescriptorHeapUAVDescriptorHandleFromStart(); }

	template<typename RenderPass>
	[[nodiscard]] inline RenderPass* GetRenderPass() const { return pRenderGraph->GetRenderPass<RenderPass>(); }

	inline auto GetCurrentSwapChainBuffer() const { return pRenderDevice->GetTexture(pRenderDevice->SwapChainTextures[pRenderDevice->FrameIndex]); }
	inline auto GetCurrentSwapChainRTV() const { return pRenderDevice->SwapChainRenderTargetViews[pRenderDevice->FrameIndex]; }
private:
	RenderDevice* pRenderDevice;
	RenderGraph* pRenderGraph;
};

class RenderGraph
{
public:
	struct Settings
	{
		static constexpr bool MultiThreaded = true;
	};

	RenderGraph(RenderDevice* pRenderDevice, GpuScene* pGpuScene);

	RenderResourceHandle GetNonTransientResource(const std::string& Name)
	{
		auto iter = m_NonTransientResources.find(Name);
		if (iter != m_NonTransientResources.end())
		{
			return iter->second;
		}
		return RenderResourceHandle();
	}

	void AddNonTransientResource(const std::string& Name, RenderResourceHandle Handle)
	{
		auto [iter, success] = m_NonTransientResources.emplace(Name, Handle);
		if (!success)
		{
			throw std::logic_error("The name has been taken");
		}
	}

	bool RemoveNonTransientResource(const std::string& Name)
	{
		auto iter = m_NonTransientResources.find(Name);
		if (iter != m_NonTransientResources.end())
		{
			m_NonTransientResources.erase(iter);
			return true;
		}
		return false;
	}

	template<typename RenderPass>
	RenderPass* GetRenderPass() const;

	void AddRenderPass(RenderPass* pIRenderPass);

	// Only call once
	void Initialize();

	// Call every frame
	void RenderGui();
	void Execute();
	void ExecuteCommandContexts(Gui* pGui);

	void ThreadBarrier();

	void Resize(UINT Width, UINT Height);
private:
	RenderDevice* pRenderDevice;
	GpuScene* pGpuScene;

	ThreadPool m_ThreadPool;
	std::vector<std::future<void>> m_Futures;

	UINT m_NumRenderPasses;
	std::vector<std::unique_ptr<RenderPass>> m_RenderPasses;
	std::vector<std::reference_wrapper<const std::type_info>> m_RenderPassDataIDs;
	std::vector<CommandContext*> m_CommandContexts;

	std::unordered_map<std::string, RenderResourceHandle> m_NonTransientResources;
};

#include "RenderGraph.inl"