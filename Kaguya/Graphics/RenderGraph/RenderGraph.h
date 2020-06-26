#pragma once
#include <memory>
#include <vector>
#include <string>
#include <functional>

#include "../RenderDevice.h"
#include "../RenderResourceHandle.h"

#include "RenderPass.h"

enum class RenderBindFlags
{
	ShaderResource,
	UnorderedAccess
};

class RenderGraph
{
public:
	RenderGraph(RenderDevice& RenderDevice);

	template<RenderPassType Type, typename Data, typename RenderPassSetupCallback = RenderPass<Type, Data>::SetupCallback, typename RenderPassExecuteCallback = RenderPass<Type, Data>::ExecuteCallback>
	RenderPass<Type, Data>* AddRenderPass(const char* pName, RenderPassSetupCallback SetupCallback, RenderPassExecuteCallback ExecuteCallback);

	void Setup();
	void Execute();

	void Debug();
private:
	RenderDevice& m_RenderDevice;
	RenderGraphRegistry m_RenderGraphRegistry;
	std::vector<std::unique_ptr<IRenderPass>> m_RenderPasses;
};

#include "RenderGraph.inl"