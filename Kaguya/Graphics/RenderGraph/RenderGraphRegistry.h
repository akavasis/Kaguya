#pragma once
#include "../RenderResourceHandle.h"

class RenderDevice;
class Buffer;
class Texture;
class Heap;
class RootSignature;
class GraphicsPipelineState;
class ComputePipelineState;

class RenderGraphRegistry
{
public:
	RenderGraphRegistry(RenderDevice& RenderDevice);

	Buffer* GetBuffer(const RenderResourceHandle& RenderResourceHandle) const;
	Texture* GetTexture(const RenderResourceHandle& RenderResourceHandle) const;
	Heap* GetHeap(const RenderResourceHandle& RenderResourceHandle) const;
	RootSignature* GetRootSignature(const RenderResourceHandle& RenderResourceHandle) const;
	GraphicsPipelineState* GetGraphicsPSO(const RenderResourceHandle& RenderResourceHandle) const;
	ComputePipelineState* GetComputePSO(const RenderResourceHandle& RenderResourceHandle) const;
private:
	RenderDevice& m_RenderDevice;
};