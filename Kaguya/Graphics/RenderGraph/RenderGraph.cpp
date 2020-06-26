#include "pch.h"
#include "RenderGraph.h"

RenderGraph::RenderGraph(RenderDevice& RenderDevice)
	: m_RenderDevice(RenderDevice),
	m_RenderGraphRegistry(RenderDevice)
{
}

void RenderGraph::Setup()
{
	for (auto& renderPass : m_RenderPasses)
	{
		renderPass->Setup(m_RenderDevice);
	}
}

void RenderGraph::Execute()
{
	for (auto& renderPass : m_RenderPasses)
	{
		renderPass->Execute(m_RenderGraphRegistry);
	}
}

void RenderGraph::Debug()
{
}
