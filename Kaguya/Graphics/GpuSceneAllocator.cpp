#include "pch.h"
#include "GpuSceneAllocator.h"

GpuSceneAllocator::GpuSceneAllocator(RenderDevice& RefRenderDevice)
	: m_pScene(nullptr),
	m_Staged(false),
	m_GpuTextureAllocator(RefRenderDevice),
	m_GpuBufferAllocator(256_MiB, 256_MiB, RefRenderDevice)
{
}

void GpuSceneAllocator::SetScene(Scene* pScene)
{
	if (m_pScene != pScene)
	{
		m_pScene = pScene;
		m_Staged = false;
	}
}

void GpuSceneAllocator::Stage(RenderCommandContext* pRenderCommandContext)
{
	if (m_pScene && !m_Staged)
	{
		m_GpuTextureAllocator.Stage(m_pScene, pRenderCommandContext);
		m_GpuBufferAllocator.Stage(m_pScene, pRenderCommandContext);
	}
}

void GpuSceneAllocator::Bind(RenderCommandContext* pRenderCommandContext)
{
	m_GpuTextureAllocator.Bind(pRenderCommandContext);
	m_GpuBufferAllocator.Bind(pRenderCommandContext);
}