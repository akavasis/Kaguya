#include "pch.h"
#include "GpuScene.h"

GpuScene::GpuScene(RenderDevice* pRenderDevice)
	: m_GpuBufferAllocator(pRenderDevice, 50_MiB, 50_MiB, 64_KiB),
	m_GpuTextureAllocator(pRenderDevice, 100)
{
}

void GpuScene::SetScene(const Scene* pScene)
{

}

void GpuScene::Reset()
{

}

void GpuScene::Upload(CommandContext* pCommandContext)
{

}
