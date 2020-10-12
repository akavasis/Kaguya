#include "pch.h"
#include "PostProcess.h"

#include "Accumulation.h"

PostProcess::PostProcess(UINT Width, UINT Height)
	: RenderPass("Post Process",
		{ Width, Height, RendererFormats::HDRBufferFormat })
{

}

void PostProcess::ScheduleResource(ResourceScheduler* pResourceScheduler)
{
	pResourceScheduler->AllocateTexture(Resource::Type::Texture2D, [&](TextureProxy& proxy)
	{
		proxy.SetFormat(Properties.Format);
		proxy.SetWidth(Properties.Width);
		proxy.SetHeight(Properties.Height);
		proxy.BindFlags = Resource::BindFlags::UnorderedAccess;
		proxy.InitialState = Resource::State::UnorderedAccess;
	});

	const UINT bloomWidth = Properties.Width > 2560u ? 1280u : 640u;
	const UINT bloomHeight = Properties.Height > 1440u ? 768u : 384u;
	UINT denominatorIndex = 0;
	const UINT denominators[EResources::NumResources / 2] = { 1, 2, 4, 8, 16 };
	for (size_t i = 1; i < EResources::NumResources; i += 2, denominatorIndex++)
	{
		pResourceScheduler->AllocateTexture(Resource::Type::Texture2D, [&](TextureProxy& proxy)
		{
			proxy.SetFormat(Properties.Format);
			proxy.SetWidth(bloomWidth / denominators[denominatorIndex]);
			proxy.SetHeight(bloomHeight / denominators[denominatorIndex]);
			proxy.BindFlags = Resource::BindFlags::UnorderedAccess;
			proxy.InitialState = Resource::State::UnorderedAccess;
		});

		pResourceScheduler->AllocateTexture(Resource::Type::Texture2D, [&](TextureProxy& proxy)
		{
			proxy.SetFormat(Properties.Format);
			proxy.SetWidth(bloomWidth / denominators[denominatorIndex]);
			proxy.SetHeight(bloomHeight / denominators[denominatorIndex]);
			proxy.BindFlags = Resource::BindFlags::UnorderedAccess;
			proxy.InitialState = Resource::State::UnorderedAccess;
		});
	}
}

void PostProcess::InitializeScene(GpuScene* pGpuScene, RenderDevice* pRenderDevice)
{

}

void PostProcess::RenderGui()
{
	if (ImGui::TreeNode(Name.data()))
	{
		if (ImGui::Button("Restore Defaults"))
		{
			Settings = SSettings();
		}

		ImGui::Checkbox("Apply Bloom", &Settings.ApplyBloom);
		if (ImGui::TreeNode("Bloom"))
		{
			ImGui::SliderFloat("Threshold", &Settings.Bloom.Threshold, 0.0f, 8.0f);
			ImGui::SliderFloat("Intensity", &Settings.Bloom.Intensity, 0.0f, 2.0f);
			ImGui::TreePop();
		}

		if (ImGui::TreeNode("Tonemapping"))
		{
			ImGui::SliderFloat("Exposure", &Settings.Tonemapping.Exposure, 0.1f, 10.0f);
			//ImGui::SliderFloat("Gamma", &Settings.Tonemapping.Gamma, 0.1f, 4.0f);
			ImGui::TreePop();
		}

		ImGui::TreePop();
	}
}

void PostProcess::Execute(RenderContext& RenderContext, RenderGraph* pRenderGraph)
{
	if (Settings.ApplyBloom)
		ApplyBloom(RenderContext, pRenderGraph);
	ApplyTonemappingToSwapChain(RenderContext, pRenderGraph);
}

void PostProcess::StateRefresh()
{

}

void PostProcess::ApplyBloom(RenderContext& RenderContext, RenderGraph* pRenderGraph)
{
	/*
		Bloom PP is based on Microsoft's DirectX-Graphics-Samples's MiniEngine.
		https://github.com/microsoft/DirectX-Graphics-Samples/tree/master/MiniEngine
	*/
	PIXMarker(RenderContext->GetD3DCommandList(), L"Bloom");

	auto pAccumulationRenderPass = pRenderGraph->GetRenderPass<Accumulation>();
	Descriptor InputSRV = RenderContext.GetShaderResourceView(pAccumulationRenderPass->Resources[Accumulation::EResources::RenderTarget]);

	const UINT bloomWidth = Properties.Width > 2560u ? 1280u : 640u;
	const UINT bloomHeight = Properties.Height > 1440u ? 768u : 384u;
	// Bloom Mask
	{
		PIXMarker(RenderContext->GetD3DCommandList(), L"Bloom Mask");

		auto pOutput = RenderContext.GetTexture(Resources[EResources::BloomRenderTarget1a]);

		struct BloomMask_Settings
		{
			float2 InverseOutputSize;
			float Threshold;
			uint InputIndex;
			uint OutputIndex;
		} settings;
		settings.InverseOutputSize = { 1.0f / bloomWidth, 1.0f / bloomHeight };
		settings.Threshold = Settings.Bloom.Threshold;
		settings.InputIndex = InputSRV.HeapIndex;
		settings.OutputIndex = RenderContext.GetUnorderedAccessView(Resources[EResources::BloomRenderTarget1a]).HeapIndex;

		RenderContext->TransitionBarrier(pOutput, Resource::State::UnorderedAccess);

		RenderContext.SetPipelineState(ComputePSOs::PostProcess_BloomMask);

		RenderContext->SetComputeRoot32BitConstants(0, 5, &settings, 0);

		RenderContext->Dispatch2D(bloomWidth, bloomHeight, 8, 8);

		RenderContext->TransitionBarrier(pOutput, Resource::State::NonPixelShaderResource);
	}

	// Bloom Downsample
	{
		PIXMarker(RenderContext->GetD3DCommandList(), L"Bloom Downsample");

		auto pOutput1 = RenderContext.GetTexture(Resources[EResources::BloomRenderTarget2a]);

		struct BloomDownsample_Settings
		{
			float2 InverseOutputSize;
			uint BloomIndex;
			uint Output1Index;
			uint Output2Index;
			uint Output3Index;
			uint Output4Index;
		} settings;

		settings.InverseOutputSize = { 1.0f / bloomWidth, 1.0f / bloomHeight };
		settings.BloomIndex = RenderContext.GetShaderResourceView(Resources[EResources::BloomRenderTarget1a]).HeapIndex;
		settings.Output1Index = RenderContext.GetUnorderedAccessView(Resources[EResources::BloomRenderTarget2a]).HeapIndex;
		settings.Output2Index = RenderContext.GetUnorderedAccessView(Resources[EResources::BloomRenderTarget3a]).HeapIndex;
		settings.Output3Index = RenderContext.GetUnorderedAccessView(Resources[EResources::BloomRenderTarget4a]).HeapIndex;
		settings.Output4Index = RenderContext.GetUnorderedAccessView(Resources[EResources::BloomRenderTarget5a]).HeapIndex;

		RenderContext.TransitionBarrier(Resources[EResources::BloomRenderTarget2a], Resource::State::UnorderedAccess);
		RenderContext.TransitionBarrier(Resources[EResources::BloomRenderTarget3a], Resource::State::UnorderedAccess);
		RenderContext.TransitionBarrier(Resources[EResources::BloomRenderTarget4a], Resource::State::UnorderedAccess);
		RenderContext.TransitionBarrier(Resources[EResources::BloomRenderTarget5a], Resource::State::UnorderedAccess);

		RenderContext.SetPipelineState(ComputePSOs::PostProcess_BloomDownsample);

		RenderContext->SetComputeRoot32BitConstants(0, 7, &settings, 0);

		RenderContext->Dispatch2D(bloomWidth / 2, bloomHeight / 2, 8, 8);

		RenderContext.TransitionBarrier(Resources[EResources::BloomRenderTarget2a], Resource::State::NonPixelShaderResource);
		RenderContext.TransitionBarrier(Resources[EResources::BloomRenderTarget3a], Resource::State::NonPixelShaderResource);
		RenderContext.TransitionBarrier(Resources[EResources::BloomRenderTarget4a], Resource::State::NonPixelShaderResource);
		RenderContext.TransitionBarrier(Resources[EResources::BloomRenderTarget5a], Resource::State::NonPixelShaderResource);
	}

	// Upsample Blur Accumulation
	{
		// Blur then upsample and blur 4 times
		Blur(EResources::BloomRenderTarget5a, EResources::BloomRenderTarget5b, RenderContext);
		UpsampleBlurAccumulation(EResources::BloomRenderTarget4a, EResources::BloomRenderTarget5b, EResources::BloomRenderTarget4b, RenderContext);
		UpsampleBlurAccumulation(EResources::BloomRenderTarget3a, EResources::BloomRenderTarget4b, EResources::BloomRenderTarget3b, RenderContext);
		UpsampleBlurAccumulation(EResources::BloomRenderTarget2a, EResources::BloomRenderTarget3b, EResources::BloomRenderTarget2b, RenderContext);
		UpsampleBlurAccumulation(EResources::BloomRenderTarget1a, EResources::BloomRenderTarget2b, EResources::BloomRenderTarget1b, RenderContext);
	}

	// Bloom Composition
	{
		PIXMarker(RenderContext->GetD3DCommandList(), L"Bloom Composition");

		auto pBloom = RenderContext.GetTexture(Resources[EResources::BloomRenderTarget1b]);
		auto pOutput = RenderContext.GetTexture(Resources[EResources::RenderTarget]);

		struct BloomComposition_Settings
		{
			float2 InverseOutputSize;
			float Intensity;
			uint InputIndex;
			uint BloomIndex;
			uint OutputIndex;
		} settings;
		settings.InverseOutputSize = { 1.0f / pOutput->GetWidth(), 1.0f / pOutput->GetHeight() };
		settings.Intensity = Settings.Bloom.Intensity;
		settings.InputIndex = InputSRV.HeapIndex;
		settings.BloomIndex = RenderContext.GetShaderResourceView(Resources[EResources::BloomRenderTarget1b]).HeapIndex;
		settings.OutputIndex = RenderContext.GetUnorderedAccessView(Resources[EResources::RenderTarget]).HeapIndex;

		RenderContext.TransitionBarrier(Resources[EResources::RenderTarget], Resource::State::UnorderedAccess);

		RenderContext.SetPipelineState(ComputePSOs::PostProcess_BloomComposition);

		RenderContext->SetComputeRoot32BitConstants(0, 6, &settings, 0);

		RenderContext->Dispatch2D(pOutput->GetWidth(), pOutput->GetHeight(), 8, 8);

		RenderContext.TransitionBarrier(Resources[EResources::RenderTarget], Resource::State::PixelShaderResource);
	}
}

void PostProcess::ApplyTonemappingToSwapChain(RenderContext& RenderContext, RenderGraph* pRenderGraph)
{
	PIXMarker(RenderContext->GetD3DCommandList(), L"Tonemap");

	auto pAccumulationRenderPass = pRenderGraph->GetRenderPass<Accumulation>();

	struct Tonemapping_Settings
	{
		float Exposure;
		uint InputIndex;
	} settings;
	settings.Exposure = Settings.Tonemapping.Exposure;
	settings.InputIndex = Settings.ApplyBloom ? RenderContext.GetShaderResourceView(Resources[EResources::RenderTarget]).HeapIndex :
		RenderContext.GetShaderResourceView(pAccumulationRenderPass->Resources[Accumulation::EResources::RenderTarget]).HeapIndex;

	auto pDestination = RenderContext.GetTexture(RenderContext.GetCurrentSwapChainResourceHandle());
	auto DestinationRTV = RenderContext.GetRenderTargetView(RenderContext.GetCurrentSwapChainResourceHandle());

	RenderContext->TransitionBarrier(pDestination, Resource::State::RenderTarget);

	RenderContext.SetPipelineState(GraphicsPSOs::PostProcess_Tonemapping);
	RenderContext->SetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);

	RenderContext->SetGraphicsRoot32BitConstants(0, 2, &settings, 0);

	D3D12_VIEWPORT vp;
	vp.TopLeftX = vp.TopLeftY = 0.0f;
	vp.MinDepth = 0.0f;
	vp.MaxDepth = 1.0f;
	vp.Width = pDestination->GetWidth();
	vp.Height = pDestination->GetHeight();

	D3D12_RECT sr;
	sr.left = sr.top = 0;
	sr.right = pDestination->GetWidth();
	sr.bottom = pDestination->GetHeight();

	RenderContext->SetViewports(1, &vp);
	RenderContext->SetScissorRects(1, &sr);

	RenderContext->SetRenderTargets(1, DestinationRTV, TRUE, Descriptor());
	RenderContext->DrawInstanced(3, 1, 0, 0);

	RenderContext->TransitionBarrier(pDestination, Resource::State::Present);
}

void PostProcess::Blur(size_t Input, size_t Output, RenderContext& RenderContext)
{
	PIXMarker(RenderContext->GetD3DCommandList(), L"Bloom Blur");

	auto pOutput = RenderContext.GetTexture(Resources[Output]);

	struct BloomBlur_Settings
	{
		float2 InverseOutputSize;
		uint InputIndex;
		uint OutputIndex;
	} settings;
	settings.InverseOutputSize = { 1.0f / pOutput->GetWidth(), 1.0f / pOutput->GetHeight() };
	settings.InputIndex = RenderContext.GetShaderResourceView(Resources[Input]).HeapIndex;
	settings.OutputIndex = RenderContext.GetUnorderedAccessView(Resources[Output]).HeapIndex;

	RenderContext.TransitionBarrier(Resources[Input], Resource::State::NonPixelShaderResource);
	RenderContext.TransitionBarrier(Resources[Output], Resource::State::UnorderedAccess);

	RenderContext.SetPipelineState(ComputePSOs::PostProcess_BloomBlur);

	RenderContext->SetComputeRoot32BitConstants(0, 4, &settings, 0);

	RenderContext->Dispatch2D(pOutput->GetWidth(), pOutput->GetHeight(), 8, 8);

	RenderContext.TransitionBarrier(Resources[Output], Resource::State::NonPixelShaderResource);
}

void PostProcess::UpsampleBlurAccumulation(size_t HighResolution, size_t LowResolution, size_t Output, RenderContext& RenderContext)
{
	PIXMarker(RenderContext->GetD3DCommandList(), L"Bloom Upsample Blur Accumulation");

	auto pOutput = RenderContext.GetTexture(Resources[Output]);

	struct BloomUpsampleBlurAccumulation_Settings
	{
		float2 InverseOutputSize;
		float UpsampleInterpolationFactor;
		uint HighResolutionIndex;
		uint LowResolutionIndex;
		uint OutputIndex;
	} settings;
	settings.InverseOutputSize = { 1.0f / pOutput->GetWidth(), 1.0f / pOutput->GetHeight() };
	settings.UpsampleInterpolationFactor = 1.0f;
	settings.HighResolutionIndex = RenderContext.GetShaderResourceView(Resources[HighResolution]).HeapIndex;
	settings.LowResolutionIndex = RenderContext.GetShaderResourceView(Resources[LowResolution]).HeapIndex;
	settings.OutputIndex = RenderContext.GetUnorderedAccessView(Resources[Output]).HeapIndex;

	RenderContext.TransitionBarrier(Resources[HighResolution], Resource::State::NonPixelShaderResource);
	RenderContext.TransitionBarrier(Resources[LowResolution], Resource::State::NonPixelShaderResource);
	RenderContext.TransitionBarrier(Resources[Output], Resource::State::UnorderedAccess);

	RenderContext.SetPipelineState(ComputePSOs::PostProcess_BloomUpsampleBlurAccumulation);

	RenderContext->SetComputeRoot32BitConstants(0, 6, &settings, 0);

	RenderContext->Dispatch2D(pOutput->GetWidth(), pOutput->GetHeight(), 8, 8);

	RenderContext.TransitionBarrier(Resources[Output], Resource::State::NonPixelShaderResource);
}