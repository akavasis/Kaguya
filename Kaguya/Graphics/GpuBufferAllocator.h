#pragma once
#include "RenderDevice.h"
#include "../Core/Allocator/VariableSizedAllocator.h"
#include "Scene/Scene.h"

class GpuBufferAllocator
{
public:
	GpuBufferAllocator(std::size_t VertexBufferByteSize, std::size_t IndexBufferByteSize, std::size_t ConstantBufferByteSize, RenderDevice* pRenderDevice);

	inline auto GetVertexBuffer() const { return m_pVertexBuffer; }
	inline auto GetIndexBuffer() const { return m_pIndexBuffer; }
	inline auto GetConstantBuffer() const { return m_pConstantBuffer; }

	void Initialize(RenderCommandContext* pRenderCommandContext);
	void StageSkybox(Scene& Scene, RenderCommandContext* pRenderCommandContext);
	void Stage(Scene& Scene, RenderCommandContext* pRenderCommandContext);
	void Update(Scene& Scene);
	void Bind(RenderCommandContext* pRenderCommandContext) const;
private:
	RenderDevice* m_pRenderDevice;

	VariableSizedAllocator m_VertexBufferAllocator;
	VariableSizedAllocator m_IndexBufferAllocator;
	VariableSizedAllocator m_ConstantBufferAllocator;

	RenderResourceHandle m_VertexBuffer;
	RenderResourceHandle m_IndexBuffer;
	RenderResourceHandle m_ConstantBuffer;
	RenderResourceHandle m_UploadVertexBuffer;
	RenderResourceHandle m_UploadIndexBuffer;
	Buffer* m_pVertexBuffer;
	Buffer* m_pIndexBuffer;
	Buffer* m_pConstantBuffer;
	Buffer* m_pUploadVertexBuffer;
	Buffer* m_pUploadIndexBuffer;
};