#pragma once
#include <memory>
#include <vector>
#include <functional>
#include <unordered_set>
#include "Core/ThreadPool.h"
#include "RenderDevice.h"
#include "Scene/Scene.h"

// Forward decl
class RenderGraphRegistry;
class RenderPassBase;
class RenderGraph;

enum class RenderPassType
{
	Graphics,
	Compute,
	Copy
};

class RenderPassBase
{
public:
	RenderPassBase(RenderPassType Type);
	virtual ~RenderPassBase() = default;

	virtual void Setup(RenderDevice*) = 0;
	virtual void Execute(const Scene&, RenderGraphRegistry&, CommandContext*) = 0;
	virtual void Resize(UINT, UINT, RenderDevice*) = 0;

	bool Enabled;
	const RenderPassType Type;
	std::vector<RenderResourceHandle> Outputs;
	std::vector<DescriptorAllocation> ResourceViews;
};

struct VoidRenderPassData {};

template<typename Data>
class RenderPass : public RenderPassBase
{
public:
	using ExecuteCallback = Delegate<void(const RenderPass<Data>&, const Scene&, RenderGraphRegistry&, CommandContext*)>;
	using SetupCallback = Delegate<ExecuteCallback(RenderPass<Data>&, RenderDevice*)>;
	using ResizeCallback = Delegate<void(RenderPass<Data>&, UINT, UINT, RenderDevice*)>;

	RenderPass(RenderPassType Type, SetupCallback&& RenderPassSetupCallback, ResizeCallback&& RenderPassResizeCallback);
	~RenderPass() override = default;

	void Setup(RenderDevice* pRenderDevice) override;
	void Execute(const Scene& Scene, RenderGraphRegistry& RenderGraphRegistry, CommandContext* pCommandContext) override;
	void Resize(UINT Width, UINT Height, RenderDevice* pRenderDevice) override;

	Data Data;
private:
	friend class RenderGraph;

	SetupCallback m_SetupCallback;
	ExecuteCallback m_ExecuteCallback;
	ResizeCallback m_ResizeCallback;
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

	template<typename Data>
	[[nodiscard]] inline RenderPass<Data>* GetRenderPass() { return pRenderGraph->GetRenderPass<Data>(); }
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

	RenderGraph(RenderDevice* pRenderDevice);

	inline auto GetFrameIndex() const { return m_FrameIndex; }

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

	template<typename Data>
	RenderPass<Data>* GetRenderPass();

	template<typename Data, typename SetupCallback = RenderPass<Data>::SetupCallback, typename ResizeCallback = RenderPass<Data>::ResizeCallback>
	RenderPass<Data>* AddRenderPass(RenderPassType Type, typename SetupCallback&& RenderPassSetupCallback, typename ResizeCallback&& RenderPassResizeCallback);

	// Only call once
	void Setup();

	// Call every frame
	void Execute(UINT FrameIndex, Scene& Scene);
	void ExecuteCommandContexts();

	void ThreadBarrier();

	void Resize(UINT Width, UINT Height);
private:
	RenderDevice* pRenderDevice;
	std::unique_ptr<ThreadPool> m_ThreadPool;
	std::vector<std::future<void>> m_Futures;

	UINT m_NumRenderPasses;
	std::vector<std::unique_ptr<RenderPassBase>> m_RenderPasses;
	std::vector<std::reference_wrapper<const std::type_info>> m_RenderPassDataIDs;
	std::vector<CommandContext*> m_CommandContexts;

	std::unordered_map<std::string, RenderResourceHandle> m_NonTransientResources;
	UINT m_FrameIndex;
};

#include "RenderGraph.inl"