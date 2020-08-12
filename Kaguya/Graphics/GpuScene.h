#pragma once
#include "Scene/Scene.h"
#include "GpuBufferAllocator.h"
#include "GpuTextureAllocator.h"
#include "RenderDevice.h"

class GpuScene
{
public:
	GpuScene(RenderDevice* pRenderDevice);

	void SetScene(const Scene* pScene);
	void Reset();

	void Upload(CommandContext* pCommandContext);
private:
	const Scene* m_pScene;
	GpuBufferAllocator m_GpuBufferAllocator;
	GpuTextureAllocator m_GpuTextureAllocator;
};