#pragma once
#include "Scene/Scene.h"
#include "RenderDevice.h"
#include "GpuTextureAllocator.h"
#include "GpuBufferAllocator.h"

class GpuSceneAllocator
{
public:
	GpuSceneAllocator(RenderDevice& RefRenderDevice);

	void SetScene(Scene* pScene);

	void Stage(RenderCommandContext* pRenderCommandContext);
	void Bind(RenderCommandContext* pRenderCommandContext);
private:
	Scene* m_pScene;
	bool m_Staged;
	GpuTextureAllocator m_GpuTextureAllocator;
	GpuBufferAllocator m_GpuBufferAllocator;
};