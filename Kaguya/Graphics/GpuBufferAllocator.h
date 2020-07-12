#pragma once

#include "RenderDevice.h"
#include "../Core/Allocator/VariableSizedAllocator.h"
#include "Scene/Scene.h"

class GpuBufferAllocator
{
public:
	GpuBufferAllocator(std::size_t VertexBufferByteSize, std::size_t IndexBufferByteSize, RenderDevice& RefRenderDevice);

	void Stage(Scene* pScene, RenderCommandContext* pRenderCommandContext);
	void Bind(RenderCommandContext* pRenderCommandContext) const;
private:
	RenderDevice& m_RefRenderDevice;

	VariableSizedAllocator m_VertexBufferAllocator;
	VariableSizedAllocator m_IndexBufferAllocator;

	RenderResourceHandle m_VertexBuffer;
	RenderResourceHandle m_IndexBuffer;
	RenderResourceHandle m_UploadVertexBuffer;
	RenderResourceHandle m_UploadIndexBuffer;
	Buffer* m_pVertexBuffer;
	Buffer* m_pIndexBuffer;
	Buffer* m_pUploadVertexBuffer;
	Buffer* m_pUploadIndexBuffer;
};