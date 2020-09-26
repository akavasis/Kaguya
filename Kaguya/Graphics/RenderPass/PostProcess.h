#pragma once
#include "Graphics/Scene/Scene.h"
#include "Graphics/GpuScene.h"
#include "Graphics/RenderGraph.h"
#include "Graphics/RendererRegistry.h"

class PostProcess : public RenderPass
{
public:
	struct EResources
	{
		enum
		{
			RenderTarget,
			BloomRenderTarget1a,	// 1
			BloomRenderTarget1b,	// 1
			BloomRenderTarget2a,	// 1/2
			BloomRenderTarget2b,	// 1/2
			BloomRenderTarget3a,	// 1/4
			BloomRenderTarget3b,	// 1/4
			BloomRenderTarget4a,	// 1/8
			BloomRenderTarget4b,	// 1/8
			BloomRenderTarget5a,	// 1/16
			BloomRenderTarget5b,	// 1/16
			NumResources
		};
	};

	struct EResourceViews
	{
		enum
		{
			RenderTargetSRV,
			RenderTargetUAV,
			BloomUAVs,
			BloomSRVs,
			NumResourceViews
		};
	};

	struct SSettings
	{
		bool ApplyBloom = true;
		struct Bloom
		{
			float Threshold = 4.0f;
			float Intensity = 0.03f;
		} Bloom;
		struct Tonemapping
		{
			float Exposure = 0.5f;
			float Gamma = 2.2f;
		} Tonemapping;
	};

	PostProcess(Descriptor Input, UINT Width, UINT Height);
	virtual ~PostProcess() override;
protected:
	virtual bool Initialize(RenderDevice* pRenderDevice) override;
	virtual void InitializeScene(GpuScene* pGpuScene, RenderDevice* pRenderDevice) override;
	virtual void RenderGui() override;
	virtual void Execute(RenderGraphRegistry& RenderGraphRegistry, CommandContext* pCommandContext) override;
	virtual void Resize(UINT Width, UINT Height, RenderDevice* pRenderDevice) override;
	virtual void StateRefresh() override;
private:
	void Blur(size_t Input, size_t Output, RenderGraphRegistry& RenderGraphRegistry, CommandContext* pCommandContext);
	void UpsampleBlur(size_t HighResolution, size_t LowResolution, size_t Output, RenderGraphRegistry& RenderGraphRegistry, CommandContext* pCommandContext);
	void ApplyBloom(RenderGraphRegistry& RenderGraphRegistry, CommandContext* pCommandContext);
	void ApplyTonemappingToSwapChain(RenderGraphRegistry& RenderGraphRegistry, CommandContext* pCommandContext);

	SSettings Settings;

	Descriptor Input;
};