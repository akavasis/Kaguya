#include "pch.h"
#include "PostProcess.h"

PostProcess::PostProcess(Descriptor Input, UINT Width, UINT Height)
	: RenderPass(RenderPassType::Graphics,
		{ Width, Height, RendererFormats::HDRBufferFormat },
		EResources::NumResources,
		EResourceViews::NumResourceViews),
	Input(Input)
{

}

PostProcess::~PostProcess()
{

}

bool PostProcess::Initialize(RenderDevice* pRenderDevice)
{
	Resources[EResources::RenderTarget] = pRenderDevice->CreateTexture(Resource::Type::Texture2D, [&](TextureProxy& proxy)
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
		Resources[i] = pRenderDevice->CreateTexture(Resource::Type::Texture2D, [&](TextureProxy& proxy)
		{
			proxy.SetFormat(Properties.Format);
			proxy.SetWidth(bloomWidth / denominators[denominatorIndex]);
			proxy.SetHeight(bloomHeight / denominators[denominatorIndex]);
			proxy.BindFlags = Resource::BindFlags::UnorderedAccess;
			proxy.InitialState = Resource::State::UnorderedAccess;
		});

		Resources[i + 1] = pRenderDevice->CreateTexture(Resource::Type::Texture2D, [&](TextureProxy& proxy)
		{
			proxy.SetFormat(Properties.Format);
			proxy.SetWidth(bloomWidth / denominators[denominatorIndex]);
			proxy.SetHeight(bloomHeight / denominators[denominatorIndex]);
			proxy.BindFlags = Resource::BindFlags::UnorderedAccess;
			proxy.InitialState = Resource::State::UnorderedAccess;
		});
	}

	ResourceViews[EResourceViews::RenderTargetSRV] = pRenderDevice->DescriptorAllocator.AllocateSRDescriptors(1);
	ResourceViews[EResourceViews::RenderTargetUAV] = pRenderDevice->DescriptorAllocator.AllocateUADescriptors(1);
	ResourceViews[EResourceViews::BloomUAVs] = pRenderDevice->DescriptorAllocator.AllocateUADescriptors(EResources::NumResources);
	ResourceViews[EResourceViews::BloomSRVs] = pRenderDevice->DescriptorAllocator.AllocateSRDescriptors(EResources::NumResources);

	pRenderDevice->CreateSRV(Resources[EResources::RenderTarget], ResourceViews[EResourceViews::RenderTargetSRV][EResources::RenderTarget]);
	pRenderDevice->CreateUAV(Resources[EResources::RenderTarget], ResourceViews[EResourceViews::RenderTargetUAV][EResources::RenderTarget]);
	for (size_t i = 0; i < EResources::NumResources; ++i)
	{
		pRenderDevice->CreateUAV(Resources[i], ResourceViews[EResourceViews::BloomUAVs][i]);
		pRenderDevice->CreateSRV(Resources[i], ResourceViews[EResourceViews::BloomSRVs][i]);
	}

	return true;
}

void PostProcess::InitializeScene(GpuScene* pGpuScene, RenderDevice* pRenderDevice)
{

}

void PostProcess::RenderGui()
{
	if (ImGui::TreeNode("Post Process"))
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

void PostProcess::Execute(RenderGraphRegistry& RenderGraphRegistry, CommandContext* pCommandContext)
{
	if (Settings.ApplyBloom)
		ApplyBloom(RenderGraphRegistry, pCommandContext);
	ApplyTonemappingToSwapChain(RenderGraphRegistry, pCommandContext);
}

void PostProcess::Resize(UINT Width, UINT Height, RenderDevice* pRenderDevice)
{
	RenderPass::Resize(Width, Height, pRenderDevice);

}

void PostProcess::StateRefresh()
{

}

void PostProcess::Blur(size_t Input, size_t Output, RenderGraphRegistry& RenderGraphRegistry, CommandContext* pCommandContext)
{
	PIXMarker(pCommandContext->GetD3DCommandList(), L"Bloom Blur");

	auto pInput = RenderGraphRegistry.GetTexture(Resources[Input]);
	auto pOutput = RenderGraphRegistry.GetTexture(Resources[Output]);

	auto InputSRV = ResourceViews[EResourceViews::BloomSRVs][Input];
	auto OutputUAV = ResourceViews[EResourceViews::BloomUAVs][Output];

	pCommandContext->TransitionBarrier(pInput, Resource::State::NonPixelShaderResource);
	pCommandContext->TransitionBarrier(pOutput, Resource::State::UnorderedAccess);

	pCommandContext->SetPipelineState(RenderGraphRegistry.GetComputePSO(ComputePSOs::PostProcess_BloomBlur));
	pCommandContext->SetComputeRootSignature(RenderGraphRegistry.GetRootSignature(RootSignatures::PostProcess_BloomBlur));

	struct
	{
		DirectX::XMFLOAT2 InverseOutputSize;
	} BlurSettings;
	BlurSettings.InverseOutputSize = { 1.0f / pOutput->GetWidth(), 1.0f / pOutput->GetHeight() };
	pCommandContext->SetComputeRoot32BitConstants(0, 2, &BlurSettings, 0);
	pCommandContext->SetComputeRootDescriptorTable(1, InputSRV.GPUHandle);
	pCommandContext->SetComputeRootDescriptorTable(2, OutputUAV.GPUHandle);

	UINT threadGroupCountX = Math::RoundUpAndDivide(pOutput->GetWidth(), 8);
	UINT threadGroupCountY = Math::RoundUpAndDivide(pOutput->GetHeight(), 8);
	pCommandContext->Dispatch(threadGroupCountX, threadGroupCountY, 1);

	pCommandContext->TransitionBarrier(pOutput, Resource::State::NonPixelShaderResource);
}

void PostProcess::UpsampleBlur(size_t HighResolution, size_t LowResolution, size_t Output, RenderGraphRegistry& RenderGraphRegistry, CommandContext* pCommandContext)
{
	PIXMarker(pCommandContext->GetD3DCommandList(), L"Bloom Upsample Blur");

	auto pHighResolution = RenderGraphRegistry.GetTexture(Resources[HighResolution]);
	auto pLowResolution = RenderGraphRegistry.GetTexture(Resources[LowResolution]);
	auto pOutput = RenderGraphRegistry.GetTexture(Resources[Output]);

	auto HighResolutionSRV = ResourceViews[EResourceViews::BloomSRVs][HighResolution];
	auto LowResolutionSRV = ResourceViews[EResourceViews::BloomSRVs][LowResolution];
	auto OutputUAV = ResourceViews[EResourceViews::BloomUAVs][Output];

	pCommandContext->TransitionBarrier(pHighResolution, Resource::State::NonPixelShaderResource);
	pCommandContext->TransitionBarrier(pLowResolution, Resource::State::NonPixelShaderResource);
	pCommandContext->TransitionBarrier(pOutput, Resource::State::UnorderedAccess);

	pCommandContext->SetPipelineState(RenderGraphRegistry.GetComputePSO(ComputePSOs::PostProcess_BloomUpsampleBlurAccumulation));
	pCommandContext->SetComputeRootSignature(RenderGraphRegistry.GetRootSignature(RootSignatures::PostProcess_BloomUpsampleBlurAccumulation));

	struct
	{
		DirectX::XMFLOAT2 InverseOutputSize;
		float UpsampleInterpolationFactor;
	} UpsampleBlurSettings;
	UpsampleBlurSettings.InverseOutputSize = { 1.0f / pOutput->GetWidth(), 1.0f / pOutput->GetHeight() };
	UpsampleBlurSettings.UpsampleInterpolationFactor = 1.0f;
	pCommandContext->SetComputeRoot32BitConstants(0, 3, &UpsampleBlurSettings, 0);
	pCommandContext->SetComputeRootDescriptorTable(1, HighResolutionSRV.GPUHandle);
	pCommandContext->SetComputeRootDescriptorTable(2, LowResolutionSRV.GPUHandle);
	pCommandContext->SetComputeRootDescriptorTable(3, OutputUAV.GPUHandle);

	UINT threadGroupCountX = Math::RoundUpAndDivide(pOutput->GetWidth(), 8);
	UINT threadGroupCountY = Math::RoundUpAndDivide(pOutput->GetHeight(), 8);
	pCommandContext->Dispatch(threadGroupCountX, threadGroupCountY, 1);

	pCommandContext->TransitionBarrier(pOutput, Resource::State::NonPixelShaderResource);
}

void PostProcess::ApplyBloom(RenderGraphRegistry& RenderGraphRegistry, CommandContext* pCommandContext)
{
	PIXMarker(pCommandContext->GetD3DCommandList(), L"Bloom");

	const UINT bloomWidth = Properties.Width > 2560u ? 1280u : 640u;
	const UINT bloomHeight = Properties.Height > 1440u ? 768u : 384u;
	// Mask
	{
		PIXMarker(pCommandContext->GetD3DCommandList(), L"Bloom Mask");

		auto pOutput = RenderGraphRegistry.GetTexture(Resources[EResources::BloomRenderTarget1a]);
		auto OutputUAV = ResourceViews[EResourceViews::BloomUAVs][EResources::BloomRenderTarget1a];

		pCommandContext->TransitionBarrier(pOutput, Resource::State::UnorderedAccess);

		pCommandContext->SetPipelineState(RenderGraphRegistry.GetComputePSO(ComputePSOs::PostProcess_BloomMask));
		pCommandContext->SetComputeRootSignature(RenderGraphRegistry.GetRootSignature(RootSignatures::PostProcess_BloomMask));

		struct
		{
			DirectX::XMFLOAT2 InverseOutputSize;
			float Threshold;
		} MaskSettings;
		MaskSettings.InverseOutputSize = { 1.0f / bloomWidth, 1.0f / bloomHeight };
		MaskSettings.Threshold = Settings.Bloom.Threshold;
		pCommandContext->SetComputeRoot32BitConstants(0, 3, &MaskSettings, 0);
		pCommandContext->SetComputeRootDescriptorTable(1, Input.GPUHandle);
		pCommandContext->SetComputeRootDescriptorTable(2, OutputUAV.GPUHandle);

		UINT threadGroupCountX = Math::RoundUpAndDivide(bloomWidth, 8);
		UINT threadGroupCountY = Math::RoundUpAndDivide(bloomHeight, 8);
		pCommandContext->Dispatch(threadGroupCountX, threadGroupCountY, 1);

		pCommandContext->TransitionBarrier(pOutput, Resource::State::NonPixelShaderResource);
	}

	// Downsample
	{
		PIXMarker(pCommandContext->GetD3DCommandList(), L"Bloom Downsample");

		auto pOutput1 = RenderGraphRegistry.GetTexture(Resources[EResources::BloomRenderTarget2a]);
		auto pOutput2 = RenderGraphRegistry.GetTexture(Resources[EResources::BloomRenderTarget3a]);
		auto pOutput3 = RenderGraphRegistry.GetTexture(Resources[EResources::BloomRenderTarget4a]);
		auto pOutput4 = RenderGraphRegistry.GetTexture(Resources[EResources::BloomRenderTarget5a]);

		auto Bloom = ResourceViews[EResourceViews::BloomSRVs][EResources::BloomRenderTarget1a];
		auto Output1 = ResourceViews[EResourceViews::BloomUAVs][EResources::BloomRenderTarget2a];
		auto Output2 = ResourceViews[EResourceViews::BloomUAVs][EResources::BloomRenderTarget3a];
		auto Output3 = ResourceViews[EResourceViews::BloomUAVs][EResources::BloomRenderTarget4a];
		auto Output4 = ResourceViews[EResourceViews::BloomUAVs][EResources::BloomRenderTarget5a];

		pCommandContext->TransitionBarrier(pOutput1, Resource::State::UnorderedAccess);
		pCommandContext->TransitionBarrier(pOutput2, Resource::State::UnorderedAccess);
		pCommandContext->TransitionBarrier(pOutput3, Resource::State::UnorderedAccess);
		pCommandContext->TransitionBarrier(pOutput4, Resource::State::UnorderedAccess);

		pCommandContext->SetPipelineState(RenderGraphRegistry.GetComputePSO(ComputePSOs::PostProcess_BloomDownsample));
		pCommandContext->SetComputeRootSignature(RenderGraphRegistry.GetRootSignature(RootSignatures::PostProcess_BloomDownsample));

		struct
		{
			DirectX::XMFLOAT2 InverseOutputSize;
		} DownsampleSettings;
		DownsampleSettings.InverseOutputSize = { 1.0f / bloomWidth, 1.0f / bloomHeight };
		pCommandContext->SetComputeRoot32BitConstants(0, 2, &DownsampleSettings, 0);
		pCommandContext->SetComputeRootDescriptorTable(1, Bloom.GPUHandle);
		pCommandContext->SetComputeRootDescriptorTable(2, Output1.GPUHandle);
		pCommandContext->SetComputeRootDescriptorTable(3, Output2.GPUHandle);
		pCommandContext->SetComputeRootDescriptorTable(4, Output3.GPUHandle);
		pCommandContext->SetComputeRootDescriptorTable(5, Output4.GPUHandle);

		UINT threadGroupCountX = Math::RoundUpAndDivide(bloomWidth / 2, 8);
		UINT threadGroupCountY = Math::RoundUpAndDivide(bloomHeight / 2, 8);
		pCommandContext->Dispatch(threadGroupCountX, threadGroupCountY, 1);

		pCommandContext->TransitionBarrier(pOutput1, Resource::State::NonPixelShaderResource);
		pCommandContext->TransitionBarrier(pOutput2, Resource::State::NonPixelShaderResource);
		pCommandContext->TransitionBarrier(pOutput3, Resource::State::NonPixelShaderResource);
		pCommandContext->TransitionBarrier(pOutput4, Resource::State::NonPixelShaderResource);
	}

	// Upsample Blur Accumulation
	{
		// Blur then upsample and blur 4 times
		Blur(EResources::BloomRenderTarget5a, EResources::BloomRenderTarget5b, RenderGraphRegistry, pCommandContext);
		UpsampleBlur(EResources::BloomRenderTarget4a, EResources::BloomRenderTarget5b, EResources::BloomRenderTarget4b, RenderGraphRegistry, pCommandContext);
		UpsampleBlur(EResources::BloomRenderTarget3a, EResources::BloomRenderTarget4b, EResources::BloomRenderTarget3b, RenderGraphRegistry, pCommandContext);
		UpsampleBlur(EResources::BloomRenderTarget2a, EResources::BloomRenderTarget3b, EResources::BloomRenderTarget2b, RenderGraphRegistry, pCommandContext);
		UpsampleBlur(EResources::BloomRenderTarget1a, EResources::BloomRenderTarget2b, EResources::BloomRenderTarget1b, RenderGraphRegistry, pCommandContext);
	}

	// Apply bloom
	{
		auto pBloom = RenderGraphRegistry.GetTexture(Resources[EResources::BloomRenderTarget1b]);
		auto pOutput = RenderGraphRegistry.GetTexture(Resources[EResources::RenderTarget]);

		auto BloomSRV = ResourceViews[EResourceViews::BloomSRVs][EResources::BloomRenderTarget1b];
		auto OutputUAV = ResourceViews[EResourceViews::RenderTargetUAV][EResources::RenderTarget];

		pCommandContext->TransitionBarrier(pOutput, Resource::State::UnorderedAccess);

		pCommandContext->SetPipelineState(RenderGraphRegistry.GetComputePSO(ComputePSOs::PostProcess_BloomComposition));
		pCommandContext->SetComputeRootSignature(RenderGraphRegistry.GetRootSignature(RootSignatures::PostProcess_BloomComposition));

		struct
		{
			DirectX::XMFLOAT2 InverseOutputSize;
			float Intensity;
		} BloomSettings;
		BloomSettings.InverseOutputSize = { 1.0f / pOutput->GetWidth(), 1.0f / pOutput->GetHeight() };
		BloomSettings.Intensity = Settings.Bloom.Intensity;
		pCommandContext->SetComputeRoot32BitConstants(0, 3, &BloomSettings, 0);
		pCommandContext->SetComputeRootDescriptorTable(1, Input.GPUHandle);
		pCommandContext->SetComputeRootDescriptorTable(2, BloomSRV.GPUHandle);
		pCommandContext->SetComputeRootDescriptorTable(3, OutputUAV.GPUHandle);

		UINT threadGroupCountX = Math::RoundUpAndDivide(pOutput->GetWidth(), 8);
		UINT threadGroupCountY = Math::RoundUpAndDivide(pOutput->GetHeight(), 8);
		pCommandContext->Dispatch(threadGroupCountX, threadGroupCountY, 1);

		pCommandContext->TransitionBarrier(pOutput, Resource::State::PixelShaderResource);
	}
}

void PostProcess::ApplyTonemappingToSwapChain(RenderGraphRegistry& RenderGraphRegistry, CommandContext* pCommandContext)
{
	PIXMarker(pCommandContext->GetD3DCommandList(), L"Tonemap");

	auto InputSRV = Settings.ApplyBloom ? ResourceViews[EResourceViews::RenderTargetSRV].GetStartDescriptor() : Input;
	auto pDestination = RenderGraphRegistry.GetCurrentSwapChainBuffer();
	auto DestinationRTV = RenderGraphRegistry.GetCurrentSwapChainRTV();

	pCommandContext->TransitionBarrier(pDestination, Resource::State::RenderTarget);

	pCommandContext->SetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
	pCommandContext->SetPipelineState(RenderGraphRegistry.GetGraphicsPSO(GraphicsPSOs::PostProcess_Tonemapping));
	pCommandContext->SetGraphicsRootSignature(RenderGraphRegistry.GetRootSignature(RootSignatures::PostProcess_Tonemapping));

	pCommandContext->SetGraphicsRoot32BitConstants(0, 2, &Settings.Tonemapping, 0);
	pCommandContext->SetGraphicsRootDescriptorTable(1, InputSRV.GPUHandle);

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

	pCommandContext->SetViewports(1, &vp);
	pCommandContext->SetScissorRects(1, &sr);

	pCommandContext->SetRenderTargets(1, DestinationRTV, TRUE, Descriptor());
	pCommandContext->DrawInstanced(3, 1, 0, 0);

	pCommandContext->TransitionBarrier(pDestination, Resource::State::Present);
}