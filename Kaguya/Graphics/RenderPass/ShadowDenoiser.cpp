#include "pch.h"
#include "ShadowDenoiser.h"

#include "GBuffer.h"
#include "Shading.h"

constexpr size_t NumEstimateNoiseRootConstants = 6;
constexpr size_t NumFilterNoiseRootConstants = 2;
constexpr size_t NumDenoiseRootConstants = 17;
constexpr size_t NumWeightedShadowCompositionRootConstants = 3;

ShadowDenoiser::ShadowDenoiser(UINT Width, UINT Height)
	: RenderPass("Shadow Denoiser",
		{ Width, Height, DXGI_FORMAT_UNKNOWN })
{
}

void ShadowDenoiser::InitializePipeline(RenderDevice* pRenderDevice)
{
	RootSignatures::EstimateNoise = pRenderDevice->CreateRootSignature([&](RootSignatureProxy& proxy)
	{
		proxy.AddRootConstantsParameter(RootConstants<void>(0, 0, NumEstimateNoiseRootConstants)); // Settings b0 | space0

		proxy.DenyTessellationShaderAccess();
		proxy.DenyGSAccess();
	});

	ComputePSOs::EstimateNoise = pRenderDevice->CreateComputePipelineState([=](ComputePipelineStateProxy& proxy)
	{
		proxy.pRootSignature = pRenderDevice->GetRootSignature(RootSignatures::EstimateNoise);
		proxy.pCS = &Shaders::CS::EstimateNoise;
	});

	RootSignatures::FilterNoise = pRenderDevice->CreateRootSignature([&](RootSignatureProxy& proxy)
	{
		proxy.AddRootConstantsParameter(RootConstants<void>(0, 0, NumEstimateNoiseRootConstants)); // Settings b0 | space0

		proxy.AddStaticSampler(0, D3D12_FILTER_MIN_MAG_MIP_LINEAR, D3D12_TEXTURE_ADDRESS_MODE_CLAMP, 0);

		proxy.DenyTessellationShaderAccess();
		proxy.DenyGSAccess();
	});

	ComputePSOs::FilterNoise = pRenderDevice->CreateComputePipelineState([=](ComputePipelineStateProxy& proxy)
	{
		proxy.pRootSignature = pRenderDevice->GetRootSignature(RootSignatures::FilterNoise);
		proxy.pCS = &Shaders::CS::FilterNoise;
	});

	RootSignatures::Denoise = pRenderDevice->CreateRootSignature([&](RootSignatureProxy& proxy)
	{
		proxy.AddRootConstantsParameter(RootConstants<void>(0, 0, NumDenoiseRootConstants)); // Settings b0 | space0

		proxy.DenyTessellationShaderAccess();
		proxy.DenyGSAccess();
	});

	ComputePSOs::Denoise = pRenderDevice->CreateComputePipelineState([=](ComputePipelineStateProxy& proxy)
	{
		proxy.pRootSignature = pRenderDevice->GetRootSignature(RootSignatures::Denoise);
		proxy.pCS = &Shaders::CS::Denoise;
	});

	RootSignatures::WeightedShadowComposition = pRenderDevice->CreateRootSignature([&](RootSignatureProxy& proxy)
	{
		proxy.AddRootConstantsParameter(RootConstants<void>(0, 0, NumWeightedShadowCompositionRootConstants)); // Settings b0 | space0

		proxy.DenyTessellationShaderAccess();
		proxy.DenyGSAccess();
	});

	ComputePSOs::WeightedShadowComposition = pRenderDevice->CreateComputePipelineState([=](ComputePipelineStateProxy& proxy)
	{
		proxy.pRootSignature = pRenderDevice->GetRootSignature(RootSignatures::WeightedShadowComposition);
		proxy.pCS = &Shaders::CS::WeightedShadowComposition;
	});
}

void ShadowDenoiser::ScheduleResource(ResourceScheduler* pResourceScheduler)
{
	pResourceScheduler->AllocateTexture(DeviceResource::Type::Texture2D, [&](DeviceTextureProxy& proxy)
	{
		proxy.SetFormat(DXGI_FORMAT_R8_UNORM);
		proxy.SetWidth(Properties.Width);
		proxy.SetHeight(Properties.Height);
		proxy.BindFlags = DeviceResource::BindFlags::UnorderedAccess;
		proxy.InitialState = DeviceResource::State::UnorderedAccess;
	});

	pResourceScheduler->AllocateTexture(DeviceResource::Type::Texture2D, [&](DeviceTextureProxy& proxy)
	{
		proxy.SetFormat(DXGI_FORMAT_R8_UNORM);
		proxy.SetWidth(Properties.Width);
		proxy.SetHeight(Properties.Height);
		proxy.BindFlags = DeviceResource::BindFlags::UnorderedAccess;
		proxy.InitialState = DeviceResource::State::UnorderedAccess;
	});

	for (size_t i = 0; i < 4; ++i)
	{
		// Allocate ping pong textures
		pResourceScheduler->AllocateTexture(DeviceResource::Type::Texture2D, [&](DeviceTextureProxy& proxy)
		{
			proxy.SetFormat(DXGI_FORMAT_R16G16B16A16_FLOAT);
			proxy.SetWidth(Properties.Width);
			proxy.SetHeight(Properties.Height);
			proxy.BindFlags = DeviceResource::BindFlags::UnorderedAccess;
			proxy.InitialState = DeviceResource::State::UnorderedAccess;
		});
	}

	pResourceScheduler->AllocateTexture(DeviceResource::Type::Texture2D, [&](DeviceTextureProxy& proxy)
	{
		proxy.SetFormat(DXGI_FORMAT_R32G32B32A32_FLOAT);
		proxy.SetWidth(Properties.Width);
		proxy.SetHeight(Properties.Height);
		proxy.BindFlags = DeviceResource::BindFlags::UnorderedAccess;
		proxy.InitialState = DeviceResource::State::UnorderedAccess;
	});
}

void ShadowDenoiser::InitializeScene(GpuScene* pGpuScene, RenderDevice* pRenderDevice)
{

}

void ShadowDenoiser::RenderGui()
{

}

void ShadowDenoiser::Execute(RenderContext& RenderContext, RenderGraph* pRenderGraph)
{
	PIXMarker(RenderContext->GetD3DCommandList(), L"Shadow Denoiser");

	EstimateNoise(RenderContext, pRenderGraph);
	FilterNoise(RenderContext, pRenderGraph);
	Denoise(RenderContext, pRenderGraph);
	WeightedShadowComposition(RenderContext, pRenderGraph);
}

void ShadowDenoiser::StateRefresh()
{

}

void ShadowDenoiser::EstimateNoise(RenderContext& RenderContext, RenderGraph* pRenderGraph)
{
	PIXMarker(RenderContext->GetD3DCommandList(), L"Estimate Noise");

	auto pShadingRenderPass = pRenderGraph->GetRenderPass<Shading>();

	struct RootConstants
	{
		uint Radius;
		float2 InverseOutputSize;

		uint Source0Index;
		uint Source1Index;
		uint RenderTargetIndex;
	} RCs;

	RCs.Radius = settings.Radius;
	RCs.InverseOutputSize = { 1.0f / Properties.Width, 1.0f / Properties.Height };

	RCs.Source0Index = RenderContext.GetShaderResourceView(pShadingRenderPass->Resources[Shading::EResources::StochasticShadowed]).HeapIndex;
	RCs.Source1Index = RenderContext.GetShaderResourceView(pShadingRenderPass->Resources[Shading::EResources::StochasticUnshadowed]).HeapIndex;
	RCs.RenderTargetIndex = RenderContext.GetUnorderedAccessView(Resources[EResources::IntermediateNoise]).HeapIndex;

	RenderContext.TransitionBarrier(Resources[EResources::IntermediateNoise], DeviceResource::State::UnorderedAccess);

	RenderContext.SetPipelineState(ComputePSOs::EstimateNoise);
	RenderContext.SetRoot32BitConstants(0, NumEstimateNoiseRootConstants, &RCs, 0);

	RenderContext->Dispatch2D(Properties.Width, Properties.Height, 8, 8);

	RenderContext.UAVBarrier(Resources[EResources::IntermediateNoise]);
}

void ShadowDenoiser::FilterNoise(RenderContext& RenderContext, RenderGraph* pRenderGraph)
{
	PIXMarker(RenderContext->GetD3DCommandList(), L"Filter Noise");

	struct RootConstants
	{
		uint SourceIndex;
		uint RenderTargetIndex;
	} RCs;

	RCs.SourceIndex = RenderContext.GetShaderResourceView(Resources[EResources::IntermediateNoise]).HeapIndex;
	RCs.RenderTargetIndex = RenderContext.GetUnorderedAccessView(Resources[EResources::BlurredNoise]).HeapIndex;

	RenderContext.TransitionBarrier(Resources[EResources::BlurredNoise], DeviceResource::State::UnorderedAccess);

	RenderContext.SetPipelineState(ComputePSOs::FilterNoise);
	RenderContext.SetRoot32BitConstants(0, NumFilterNoiseRootConstants, &RCs, 0);

	RenderContext->Dispatch2D(Properties.Width, Properties.Height, 8, 8);

	RenderContext.UAVBarrier(Resources[EResources::BlurredNoise]);
}

void ShadowDenoiser::Denoise(RenderContext& RenderContext, RenderGraph* pRenderGraph)
{
	PIXMarker(RenderContext->GetD3DCommandList(), L"Denoise");

	auto pGBufferRenderPass = pRenderGraph->GetRenderPass<GBuffer>();
	auto pShadingRenderPass = pRenderGraph->GetRenderPass<Shading>();

	RenderContext.SetPipelineState(ComputePSOs::Denoise);

	struct RootConstants
	{
		float2 Axis;
		float2 InverseOutputSize;

		// Bilateral filter parameters
		float DEPTH_WEIGHT;
		float NORMAL_WEIGHT;
		float PLANE_WEIGHT;
		float ANALYTIC_WEIGHT;
		float R;

		uint Source0Index;
		uint Source1Index;
		uint AnalyticIndex;
		uint NoiseEstimateIndex;
		uint NormalIndex;
		uint DepthIndex;

		uint RenderTarget0Index;
		uint RenderTarget1Index;
	} RCs;

	RCs.DEPTH_WEIGHT = settings.DepthWeight;
	RCs.NORMAL_WEIGHT = settings.NormalWeight;
	RCs.PLANE_WEIGHT = settings.PlaneWeight;
	RCs.ANALYTIC_WEIGHT = settings.AnalyticWeight;
	RCs.R = settings.Radius;

	RCs.InverseOutputSize = { 1.0f / Properties.Width, 1.0f / Properties.Height };

	RCs.AnalyticIndex = RenderContext.GetShaderResourceView(pShadingRenderPass->Resources[Shading::EResources::AnalyticUnshadowed]).HeapIndex;
	RCs.NoiseEstimateIndex = RenderContext.GetShaderResourceView(Resources[EResources::BlurredNoise]).HeapIndex;
	RCs.NormalIndex = RenderContext.GetShaderResourceView(pGBufferRenderPass->Resources[GBuffer::EResources::Normal]).HeapIndex;
	RCs.DepthIndex = RenderContext.GetShaderResourceView(pGBufferRenderPass->Resources[GBuffer::EResources::Depth]).HeapIndex;

	// Horizontal blur
	RCs.Axis = { 1.0f * settings.StepSize, 0.0f * settings.StepSize };

	RCs.Source0Index = RenderContext.GetShaderResourceView(pShadingRenderPass->Resources[Shading::EResources::StochasticShadowed]).HeapIndex;
	RCs.Source1Index = RenderContext.GetShaderResourceView(pShadingRenderPass->Resources[Shading::EResources::StochasticUnshadowed]).HeapIndex;

	RCs.RenderTarget0Index = RenderContext.GetUnorderedAccessView(Resources[EResources::Result0a]).HeapIndex;
	RCs.RenderTarget1Index = RenderContext.GetUnorderedAccessView(Resources[EResources::Result1a]).HeapIndex;

	RenderContext.TransitionBarrier(Resources[EResources::Result0a], DeviceResource::State::UnorderedAccess);
	RenderContext.TransitionBarrier(Resources[EResources::Result1a], DeviceResource::State::UnorderedAccess);

	RenderContext.SetRoot32BitConstants(0, NumDenoiseRootConstants, &RCs, 0);

	RenderContext->Dispatch2D(Properties.Width, Properties.Height, 8, 8);

	RenderContext.UAVBarrier(Resources[EResources::Result0a]);
	RenderContext.UAVBarrier(Resources[EResources::Result1a]);

	// Vertical blur
	RCs.Axis = { 0.0f * settings.StepSize, 1.0f * settings.StepSize };

	RCs.Source0Index = RenderContext.GetShaderResourceView(Resources[EResources::Result0b]).HeapIndex;
	RCs.Source1Index = RenderContext.GetShaderResourceView(Resources[EResources::Result1b]).HeapIndex;

	RCs.RenderTarget0Index = RenderContext.GetUnorderedAccessView(Resources[EResources::Result0b]).HeapIndex;
	RCs.RenderTarget1Index = RenderContext.GetUnorderedAccessView(Resources[EResources::Result1b]).HeapIndex;

	RenderContext.TransitionBarrier(Resources[EResources::Result0b], DeviceResource::State::UnorderedAccess);
	RenderContext.TransitionBarrier(Resources[EResources::Result1b], DeviceResource::State::UnorderedAccess);

	RenderContext.SetRoot32BitConstants(0, NumDenoiseRootConstants, &RCs, 0);

	RenderContext->Dispatch2D(Properties.Width, Properties.Height, 8, 8);

	RenderContext.UAVBarrier(Resources[EResources::Result0b]);
	RenderContext.UAVBarrier(Resources[EResources::Result1b]);
}

void ShadowDenoiser::WeightedShadowComposition(RenderContext& RenderContext, RenderGraph* pRenderGraph)
{
	PIXMarker(RenderContext->GetD3DCommandList(), L"Weighted Shadow Composition");

	struct RootConstants
	{
		uint Source0Index; // S_N
		uint Source1Index; // U_N
		uint RenderTargetIndex;
	} RCs;

	RCs.Source0Index = RenderContext.GetShaderResourceView(Resources[EResources::Result0b]).HeapIndex;
	RCs.Source1Index = RenderContext.GetShaderResourceView(Resources[EResources::Result1b]).HeapIndex;
	RCs.RenderTargetIndex = RenderContext.GetUnorderedAccessView(Resources[EResources::WeightedShadow]).HeapIndex;

	RenderContext.TransitionBarrier(Resources[EResources::WeightedShadow], DeviceResource::State::UnorderedAccess);

	RenderContext.SetPipelineState(ComputePSOs::WeightedShadowComposition);
	RenderContext.SetRoot32BitConstants(0, NumWeightedShadowCompositionRootConstants, &RCs, 0);

	RenderContext->Dispatch2D(Properties.Width, Properties.Height, 8, 8);

	RenderContext.UAVBarrier(Resources[EResources::WeightedShadow]);
}