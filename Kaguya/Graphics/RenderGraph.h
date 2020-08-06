#pragma once
#include <memory>
#include <vector>
#include <functional>
#include <unordered_set>
#include "../Core/ThreadPool.h"
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
	virtual void Resize(RenderDevice*) = 0;

	bool Enabled;
	const RenderPassType Type;
};

struct VoidRenderPassData {};

template<typename Data>
class RenderPass : public RenderPassBase
{
public:
	using ExecuteCallback = Delegate<void(const Data&, const Scene&, RenderGraphRegistry&, CommandContext*)>;
	using PassCallback = Delegate<ExecuteCallback(Data&, RenderDevice*)>;
	using ResizeCallback = Delegate<void(Data&, RenderDevice*)>;

	RenderPass(RenderPassType Type, PassCallback&& RenderPassPassCallback, ResizeCallback&& RenderPassResizeCallback);
	~RenderPass() override = default;

	inline auto& GetData() { return m_Data; }
	inline const auto& GetData() const { return m_Data; }

	void Setup(RenderDevice* pRenderDevice) override;
	void Execute(const Scene& Scene, RenderGraphRegistry& RenderGraphRegistry, CommandContext* pCommandContext) override;
	void Resize(RenderDevice* pRenderDevice) override;
private:
	friend class RenderGraph;

	Data m_Data;
	PassCallback m_PassCallback;
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

	[[nodiscard]] inline auto GetUniversalGpuDescriptorHeapSRVDescriptorHandleFromStart() const { return pRenderDevice->GetUniversalGpuDescriptorHeapSRVDescriptorHandleFromStart(); }

	template<typename Data>
	[[nodiscard]] inline RenderPass<Data>* GetRenderPass() { return pRenderGraph->GetRenderPass<Type, Data>(); }
private:
	RenderDevice* pRenderDevice;
	RenderGraph* pRenderGraph;
};

class RenderGraph
{
public:
	enum ExecutionPolicy
	{
		Sequenced, // Single threaded (Same thread as renderer), useful for debug, this is by default
		Parallel // Multi threaded, maximize performance
	};

	RenderGraph(RenderDevice* pRenderDevice);

	void SetExecutionPolicy(ExecutionPolicy ExecutionPolicy);

	template<typename Data>
	RenderPass<Data>* GetRenderPass();

	template<typename Data, typename PassCallback = RenderPass<Data>::PassCallback, typename ResizeCallback = RenderPass<Data>::ResizeCallback>
	RenderPass<Data>* AddRenderPass(RenderPassType Type, typename PassCallback&& RenderPassPassCallback, typename ResizeCallback&& RenderPassResizeCallback);

	// Only call once
	void Setup();

	// Call every frame
	void Execute(Scene& Scene);
	void ExecuteCommandContexts();

	void ThreadBarrier();

	void Resize();
private:
	RenderDevice* pRenderDevice;
	ExecutionPolicy m_ExecutionPolicy;
	std::unique_ptr<ThreadPool> m_ThreadPool;
	std::vector<std::future<void>> m_Futures;

	unsigned int m_NumRenderPasses;
	std::vector<std::unique_ptr<RenderPassBase>> m_RenderPasses;
	std::vector<CommandContext*> m_CommandContexts;

	std::vector<std::reference_wrapper<const std::type_info>> m_RenderPassDataIDs;
};

#include "RenderGraph.inl"