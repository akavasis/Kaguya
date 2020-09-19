#pragma once
#include "Graphics/Scene/Scene.h"
#include "Graphics/GpuScene.h"
#include "Graphics/RenderGraph.h"
#include "Graphics/RendererRegistry.h"

class Pathtracing : public IRenderPass
{
public:
	struct EOutputs
	{
		enum
		{
			RenderTarget,
		};
	};

	struct EResourceViews
	{
		enum
		{
			GeometryTable,
			RenderTarget
		};
	};

	Pathtracing(UINT Width, UINT Height, GpuBufferAllocator* pGpuBufferAllocator, GpuTextureAllocator* pGpuTextureAllocator, Buffer* pRenderPassConstantBuffer);
	virtual ~Pathtracing() override;

	virtual void Setup(RenderDevice* pRenderDevice) override;
	virtual void Update() override;
	virtual void RenderGui() override;
	virtual void Execute(const Scene& Scene, RenderGraphRegistry& RenderGraphRegistry, CommandContext* pCommandContext) override;
	virtual void Resize(UINT Width, UINT Height, RenderDevice* pRenderDevice) override;

private:
	GpuBufferAllocator* pGpuBufferAllocator;
	GpuTextureAllocator* pGpuTextureAllocator;
	Buffer* pRenderPassConstantBuffer;
};