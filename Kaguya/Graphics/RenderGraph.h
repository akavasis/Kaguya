#pragma once
#include <memory>
#include <vector>
#include <functional>
#include <unordered_set>

#include "../Core/ThreadPool.h"
#include "RenderDevice.h"
#include "RenderResourceHandle.h"
#include "Scene/Scene.h"

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
	RenderPassBase(RenderPassType RenderPassType);
	virtual ~RenderPassBase() = default;

	virtual void Setup(RenderDevice&) = 0;
	virtual void Execute(Scene&, RenderGraphRegistry&, RenderCommandContext*) = 0;

	bool Enabled;
	const RenderPassType eRenderPassType;
};

struct VoidRenderPassData {};

template<RenderPassType Type, typename Data>
class RenderPass : public RenderPassBase
{
public:
	using ExecuteCallback = Delegate<void(const Data&, Scene&, RenderGraphRegistry&, RenderCommandContext*)>;
	using PassCallback = Delegate<ExecuteCallback(Data&, RenderDevice&)>;

	RenderPass(PassCallback&& RenderPassPassCallback);
	~RenderPass() override = default;

	inline auto& GetData() { return m_Data; }
	inline const auto& GetData() const { return m_Data; }

	void Setup(RenderDevice& RefRenderDevice) override;
	void Execute(Scene& Scene, RenderGraphRegistry& RenderGraphRegistry, RenderCommandContext* RenderCommandContext) override;
private:
	friend class RenderGraph;

	Data m_Data;
	PassCallback m_PassCallback;
	ExecuteCallback m_ExecuteCallback;
};

class RenderGraphRegistry
{
public:
	RenderGraphRegistry(const RenderDevice& RefRenderDevice, const RenderGraph& RefRenderGraph)
		: m_RefRenderDevice(RefRenderDevice),
		m_RefRenderGraph(RefRenderGraph)
	{}

	[[nodiscard]] inline Buffer* GetBuffer(const RenderResourceHandle& RenderResourceHandle) const { return m_RefRenderDevice.GetBuffer(RenderResourceHandle); }
	[[nodiscard]] inline Texture* GetTexture(const RenderResourceHandle& RenderResourceHandle) const { return m_RefRenderDevice.GetTexture(RenderResourceHandle); }
	[[nodiscard]] inline Heap* GetHeap(const RenderResourceHandle& RenderResourceHandle) const { return m_RefRenderDevice.GetHeap(RenderResourceHandle); }
	[[nodiscard]] inline RootSignature* GetRootSignature(const RenderResourceHandle& RenderResourceHandle) const { return m_RefRenderDevice.GetRootSignature(RenderResourceHandle); }
	[[nodiscard]] inline GraphicsPipelineState* GetGraphicsPSO(const RenderResourceHandle& RenderResourceHandle) const { return m_RefRenderDevice.GetGraphicsPSO(RenderResourceHandle); }
	[[nodiscard]] inline ComputePipelineState* GetComputePSO(const RenderResourceHandle& RenderResourceHandle) const { return m_RefRenderDevice.GetComputePSO(RenderResourceHandle); }

	[[nodiscard]] inline auto GetUniversalGpuDescriptorHeapSRVDescriptorHandleFromStart() const { return m_RefRenderDevice.GetUniversalGpuDescriptorHeapSRVDescriptorHandleFromStart(); }

	template<RenderPassType Type, typename Data>
	[[nodiscard]] inline RenderPass<Type, Data>* GetRenderPass() { return m_RefRenderGraph.GetRenderPass<Type, Data>(); }
private:
	const RenderDevice& m_RefRenderDevice;
	const RenderGraph& m_RefRenderGraph;
};

class RenderGraph
{
public:
	enum ExecutionPolicy
	{
		Sequenced, // Single threaded (Same thread as renderer), useful for debug, this is by default
		Parallel // Multi threaded, maximize performance
	};

	RenderGraph(RenderDevice& RefRenderDevice);

	void SetExecutionPolicy(ExecutionPolicy ExecutionPolicy);

	template<RenderPassType Type, typename Data>
	RenderPass<Type, Data>* GetRenderPass();

	template<RenderPassType Type, typename Data, typename PassCallback = RenderPass<Type, Data>::PassCallback>
	RenderPass<Type, Data>* AddRenderPass(typename PassCallback&& RenderPassPassCallback);

	// Only call once
	void Setup();

	// Call every frame
	void Execute(Scene& Scene);
	void ExecuteCommandContexts();

	void ThreadBarrier();
private:
	RenderDevice& m_RefRenderDevice;
	ExecutionPolicy m_ExecutionPolicy;
	std::unique_ptr<ThreadPool> m_ThreadPool;
	std::vector<std::future<void>> m_Futures;

	unsigned int m_NumRenderPasses;
	std::vector<std::unique_ptr<RenderPassBase>> m_RenderPasses;
	std::vector<RenderCommandContext*> m_RenderContexts;

	std::vector<std::reference_wrapper<const std::type_info>> m_RenderPassDataIDs;
};

#include "RenderGraph.inl"