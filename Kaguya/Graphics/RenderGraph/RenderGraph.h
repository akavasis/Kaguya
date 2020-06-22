#pragma once
#include <memory>

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
	RenderGraph(RenderDevice& RenderDevice)
		: m_RenderDevice(RenderDevice)
	{
	}

	template<RenderPassType Type, typename Data, typename RenderPassSetupCallback = RenderPass<Type, Data>::SetupCallback, typename RenderPassExecuteCallback = RenderPass<Type, Data>::ExecuteCallback>
	RenderPass<Type, Data>* AddRenderPass(const char* pName,
		RenderPassSetupCallback SetupCallback,
		RenderPassExecuteCallback ExecuteCallback)
	{
		// Enforce execute callback to be less than 1 KB to avoid capturing huge structures by value
#define KB(x)   ((size_t) (x) << 10)
		constexpr size_t EXECUTE_CALLBACK_MEMORY_LIMIT = KB(1);
#undef KB
		static_assert(sizeof(ExecuteCallback) <= EXECUTE_CALLBACK_MEMORY_LIMIT, "ExecuteCallback's memory exceeds EXECUTE_CALLBACK_MEMORY_LIMIT");
		auto pRenderPass = std::make_unique<RenderPass<Data>>();
		pRenderPass->setupCallback = std::move(SetupCallback);
		pRenderPass->executeCallback = std::move(ExecuteCallback);
		if constexpr (Type == RenderPassType::Graphics)
			pRenderPass->commandContext = m_RenderDevice.CreateGraphicsContext();
		else if constexpr (Type == RenderPassType::Compute)
			pRenderPass->commandContext = m_RenderDevice.CreateComputeContext();
		else if constexpr (Type == RenderPassType::Copy)
			pRenderPass->commandContext = m_RenderDevice.CreateCopyContext();

		m_RenderPasses.emplace_back(std::move(pRenderPass));
		return static_cast<RenderPass<Data>*>(m_RenderPasses.back().get());
	}

	void Setup()
	{
		for (auto& renderPass : m_RenderPasses)
		{
			renderPass->Setup(m_RenderDevice);
		}
	}
	void Execute()
	{
		for (auto& renderPass : m_RenderPasses)
		{
			renderPass->Execute(m_RenderDevice);
		}
	}

	void Debug()
	{

	}
private:
	RenderDevice& m_RenderDevice;
	std::vector<std::unique_ptr<IRenderPass>> m_RenderPasses;
};