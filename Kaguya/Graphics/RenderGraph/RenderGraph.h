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

	std::vector<RenderCommandContext*> GetAllRenderCommandContext()
	{
		std::vector<RenderCommandContext*> retval;
		retval.reserve(m_NumRenderPasses);

		// TODO: Just return the fucking vector as const ref.
		for (decltype(m_NumRenderPasses) i = 0; i < m_NumRenderPasses; ++i)
		{
			// Don't return command lists from tasks that don't require to be executed.
			if (!m_ShouldExecute[i])
			{
				continue;
			}

			WaitForCompletion(i);
			retval.push_back(static_cast<T*>(m_cmd_lists[i]));
		}

		return retval;
	}

	template<RenderPassType Type, typename Data>
	RenderPass<Type, Data>* AddRenderPass(const char* pName, typename RenderPass<Type, Data>::PassCallback PassCallback);

	void Setup();
	void Execute();
private:
	RenderDevice& m_RenderDevice;
	RenderGraphRegistry m_RenderGraphRegistry;

	int m_NumRenderPasses;
	std::vector<bool> m_ShouldExecute;
	std::vector<std::unique_ptr<IRenderPass>> m_RenderPasses;
	std::vector<RenderCommandContext*> m_RenderContexts;
};

#include "RenderGraph.inl"