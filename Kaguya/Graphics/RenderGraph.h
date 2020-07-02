#pragma once
#include <memory>
#include <vector>
#include <string>
#include <functional>
#include <unordered_set>

#include "RenderDevice.h"
#include "RenderResourceHandle.h"

class RenderGraphBuilder;
class RenderGraphRegistry;
class RenderPassBase;
class RenderGraph;

class RenderGraphBuilder
{
public:
	RenderGraphBuilder(RenderPassBase* pRenderPassBase, RenderGraph* pRenderGraph);
	
	void Write();
	void Read();
	void Create();
private:
	RenderPassBase* m_pRenderPassBase;
	RenderGraph* m_RenderGraph;
};

class RenderGraphRegistry
{
public:
	RenderGraphRegistry(RenderDevice& RefRenderDevice)
		: m_RefRenderDevice(RefRenderDevice)
	{}

	[[nodiscard]] inline Buffer* GetBuffer(const RenderResourceHandle& RenderResourceHandle) const { return m_RefRenderDevice.GetBuffer(RenderResourceHandle); }
	[[nodiscard]] inline Texture* GetTexture(const RenderResourceHandle& RenderResourceHandle) const { return m_RefRenderDevice.GetTexture(RenderResourceHandle); }
	[[nodiscard]] inline Heap* GetHeap(const RenderResourceHandle& RenderResourceHandle) const { return m_RefRenderDevice.GetHeap(RenderResourceHandle); }
	[[nodiscard]] inline RootSignature* GetRootSignature(const RenderResourceHandle& RenderResourceHandle) const { return m_RefRenderDevice.GetRootSignature(RenderResourceHandle); }
	[[nodiscard]] inline GraphicsPipelineState* GetGraphicsPSO(const RenderResourceHandle& RenderResourceHandle) const { return m_RefRenderDevice.GetGraphicsPSO(RenderResourceHandle); }
	[[nodiscard]] inline ComputePipelineState* GetComputePSO(const RenderResourceHandle& RenderResourceHandle) const { return m_RefRenderDevice.GetComputePSO(RenderResourceHandle); }
private:
	RenderDevice& m_RefRenderDevice;
};

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
	virtual ~RenderPassBase() = 0;

	virtual void Setup(RenderDevice&) = 0;
	virtual void Execute(RenderGraphRegistry&, RenderCommandContext*) = 0;

	bool Enabled;
	const RenderPassType eRenderPassType;
private:
	friend RenderGraphBuilder;

	std::vector<RenderResourceHandle> m_Writes;
	std::vector<RenderResourceHandle> m_Reads;
	std::vector<RenderResourceHandle> m_Creates;
};

template<RenderPassType Type, typename Data>
class RenderPass : public RenderPassBase
{
public:
	using ExecuteCallback = std::function<void(const Data&, RenderGraphRegistry&, RenderCommandContext*)>;
	using PassCallback = std::function<ExecuteCallback(Data&, RenderDevice&)>;

	RenderPass(PassCallback PassCallback);
	~RenderPass() override;

	void Setup(RenderDevice& RefRenderDevice) override;
	void Execute(RenderGraphRegistry& RenderGraphRegistry, RenderCommandContext* RenderCommandContext) override;
private:
	friend class RenderGraph;

	Data m_Data;
	PassCallback m_PassCallback;
	ExecuteCallback m_ExecuteCallback;
};

class RenderGraph
{
public:
	RenderGraph(RenderDevice& RefRenderDevice);

	template<RenderPassType Type, typename Data>
	RenderPass<Type, Data>* GetRenderPass();

	template<RenderPassType Type, typename Data, typename PassCallback = RenderPass<Type, Data>::PassCallback>
	RenderPass<Type, Data>* AddRenderPass(const char* pName, typename PassCallback RenderPassPassCallback);

	void Setup();
	void Execute();
private:
	RenderDevice& m_RefRenderDevice;
	RenderGraphRegistry m_RenderGraphRegistry;

	std::size_t m_NumRenderPasses;
	std::vector<std::string> m_RenderPassNames;
	std::vector<std::unique_ptr<RenderPassBase>> m_RenderPasses;
	std::vector<RenderCommandContext*> m_RenderContexts;

	std::vector<std::reference_wrapper<const std::type_info>> m_RenderPassDataIDs;
};

#include "RenderGraph.inl"