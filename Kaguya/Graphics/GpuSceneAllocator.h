#pragma once
#include "Scene/Scene.h"
#include "RenderDevice.h"
#include "GpuTextureAllocator.h"
#include "GpuBufferAllocator.h"

class GpuSceneAllocator
{
public:
	GpuSceneAllocator(RenderDevice& RefRenderDevice)
		: m_GpuTextureAllocator(RefRenderDevice),
		m_GpuBufferAllocator(100_MiB, 100_MiB, RefRenderDevice)
	{
	}

	void SetScene(Scene* pScene)
	{
		m_pScene = pScene;
	}

	void Stage(RenderCommandContext* pRenderCommandContext);
private:
	Scene* m_pScene;
	GpuTextureAllocator m_GpuTextureAllocator;
	GpuBufferAllocator m_GpuBufferAllocator;
};