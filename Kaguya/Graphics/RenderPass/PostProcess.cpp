#include "pch.h"
#include "PostProcess.h"

#include "Accumulation.h"

PostProcess::PostProcess(UINT Width, UINT Height)
	: RenderPass(RenderPassType::Graphics,
		{ Width, Height, RendererFormats::HDRBufferFormat })
{

}

PostProcess::~PostProcess()
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

void PostProcess::Execute(ResourceRegistry& ResourceRegistry, CommandContext* pCommandContext)
{
	if (Settings.ApplyBloom)
		ApplyBloom(ResourceRegistry, pCommandContext);
	ApplyTonemappingToSwapChain(ResourceRegistry, pCommandContext);
}

void PostProcess::StateRefresh()
{

}

void PostProcess::Blur(size_t Input, size_t Output, ResourceRegistry& ResourceRegistry, CommandContext* pCommandContext)
{
	PIXMarker(pCommandContext->GetD3DCommandList(), L"Bloom Blur");

	auto pInput = ResourceRegistry.GetTexture(Resources[Input]);
	auto pOutput = ResourceRegistry.GetTexture(Resources[Output]);

	size_t inputSRVIndex = ResourceRegistry.GetShaderResourceDescriptorIndex(Resources[Input]);
	size_t outputUAVIndex = ResourceRegistry.GetUnorderedAccessDescriptorIndex(Resources[Output]);

	pCommandContext->TransitionBarrier(pInput, Resource::State::NonPixelShaderResource);
	pCommandContext->TransitionBarrier(pOutput, Resource::State::UnorderedAccess);

	pCommandContext->SetPipelineState(ResourceRegistry.GetComputePSO(ComputePSOs::PostProcess_BloomBlur));
	pCommandContext->SetComputeRootSignature(ResourceRegistry.GetRootSignature(RootSignatures::PostProcess_BloomBlur));

	struct
	{
		DirectX::XMFLOAT2 InverseOutputSize;
	} BlurSettings;
	BlurSettings.InverseOutputSize = { 1.0f / pOutput->GetWidth(), 1.0f / pOutput->GetHeight() };
	pCommandContext->SetComputeRoot32BitConstants(0, 2, &BlurSettings, 0);
	pCommandContext->SetComputeRootDescriptorTable(1, ResourceRegistry.ShaderResourceViews[inputSRVIndex].GPUHandle);
	pCommandContext->SetComputeRootDescriptorTable(2, ResourceRegistry.UnorderedAccessViews[outputUAVIndex].GPUHandle);

	UINT threadGroupCountX = Math::RoundUpAndDivide(pOutput->GetWidth(), 8);
	UINT threadGroupCountY = Math::RoundUpAndDivide(pOutput->GetHeight(), 8);
	pCommandContext->Dispatch(threadGroupCountX, threadGroupCountY, 1);

	pCommandContext->TransitionBarrier(pOutput, Resource::State::NonPixelShaderResource);
}

void PostProcess::UpsampleBlur(size_t HighResolution, size_t LowResolution, size_t Output, ResourceRegistry& ResourceRegistry, CommandContext* pCommandContext)
{
	PIXMarker(pCommandContext->GetD3DCommandList(), L"Bloom Upsample Blur");

	auto pHighResolution = ResourceRegistry.GetTexture(Resources[HighResolution]);
	auto pLowResolution = ResourceRegistry.GetTexture(Resources[LowResolution]);
	auto pOutput = ResourceRegistry.GetTexture(Resources[Output]);

	size_t HighResolutionSRVIndex = ResourceRegistry.GetShaderResourceDescriptorIndex(Resources[HighResolution]);
	size_t LowResolutionSRVIndex = ResourceRegistry.GetShaderResourceDescriptorIndex(Resources[LowResolution]);
	size_t OutputUAVIndex = ResourceRegistry.GetShaderResourceDescriptorIndex(Resources[Output]);

	pCommandContext->TransitionBarrier(pHighResolution, Resource::State::NonPixelShaderResource);
	pCommandContext->TransitionBarrier(pLowResolution, Resource::State::NonPixelShaderResource);
	pCommandContext->TransitionBarrier(pOutput, Resource::State::UnorderedAccess);

	pCommandContext->SetPipelineState(ResourceRegistry.GetComputePSO(ComputePSOs::PostProcess_BloomUpsampleBlurAccumulation));
	pCommandContext->SetComputeRootSignature(ResourceRegistry.GetRootSignature(RootSignatures::PostProcess_BloomUpsampleBlurAccumulation));

	struct
	{
		DirectX::XMFLOAT2 InverseOutputSize;
		float UpsampleInterpolationFactor;
	} UpsampleBlurSettings;
	UpsampleBlurSettings.InverseOutputSize = { 1.0f / pOutput->GetWidth(), 1.0f / pOutput->GetHeight() };
	UpsampleBlurSettings.UpsampleInterpolationFactor = 1.0f;
	pCommandContext->SetComputeRoot32BitConstants(0, 3, &UpsampleBlurSettings, 0);
	pCommandContext->SetComputeRootDescriptorTable(1, ResourceRegistry.ShaderResourceViews[HighResolutionSRVIndex].GPUHandle);
	pCommandContext->SetComputeRootDescriptorTable(2, ResourceRegistry.ShaderResourceViews[LowResolutionSRVIndex].GPUHandle);
	pCommandContext->SetComputeRootDescriptorTable(3, ResourceRegistry.ShaderResourceViews[OutputUAVIndex].GPUHandle);

	UINT threadGroupCountX = Math::RoundUpAndDivide(pOutput->GetWidth(), 8);
	UINT threadGroupCountY = Math::RoundUpAndDivide(pOutput->GetHeight(), 8);
	pCommandContext->Dispatch(threadGroupCountX, threadGroupCountY, 1);

	pCommandContext->TransitionBarrier(pOutput, Resource::State::NonPixelShaderResource);
}

void PostProcess::ApplyBloom(ResourceRegistry& ResourceRegistry, CommandContext* pCommandContext)
{
	PIXMarker(pCommandContext->GetD3DCommandList(), L"Bloom");

	const UINT bloomWidth = Properties.Width > 2560u ? 1280u : 640u;
	const UINT bloomHeight = Properties.Height > 1440u ? 768u : 384u;
	// Mask
	{
		PIXMarker(pCommandContext->GetD3DCommandList(), L"Bloom Mask");

		auto pAccumulationRenderPass = ResourceRegistry.GetRenderPass<Accumulation>();
		size_t InputSRVIndex = ResourceRegistry.GetShaderResourceDescriptorIndex(pAccumulationRenderPass->Resources[Accumulation::EResources::RenderTarget]);
		auto InputSRV = ResourceRegistry.ShaderResourceViews[InputSRVIndex];

		auto pOutput = ResourceRegistry.GetTexture(Resources[EResources::BloomRenderTarget1a]);
		size_t OutputUAVIndex = ResourceRegistry.GetUnorderedAccessDescriptorIndex(Resources[EResources::BloomRenderTarget1a]);

		pCommandContext->TransitionBarrier(pOutput, Resource::State::UnorderedAccess);

		pCommandContext->SetPipelineState(ResourceRegistry.GetComputePSO(ComputePSOs::PostProcess_BloomMask));
		pCommandContext->SetComputeRootSignature(ResourceRegistry.GetRootSignature(RootSignatures::PostProcess_BloomMask));

		struct
		{
			DirectX::XMFLOAT2 InverseOutputSize;
			float Threshold;
		} MaskSettings;
		MaskSettings.InverseOutputSize = { 1.0f / bloomWidth, 1.0f / bloomHeight };
		MaskSettings.Threshold = Settings.Bloom.Threshold;
		pCommandContext->SetComputeRoot32BitConstants(0, 3, &MaskSettings, 0);
		pCommandContext->SetComputeRootDescriptorTable(1, InputSRV.GPUHandle);
		pCommandContext->SetComputeRootDescriptorTable(2, ResourceRegistry.UnorderedAccessViews[OutputUAVIndex].GPUHandle);

		UINT threadGroupCountX = Math::RoundUpAndDivide(bloomWidth, 8);
		UINT threadGroupCountY = Math::RoundUpAndDivide(bloomHeight, 8);
		pCommandContext->Dispatch(threadGroupCountX, threadGroupCountY, 1);

		pCommandContext->TransitionBarrier(pOutput, Resource::State::NonPixelShaderResource);
	}

	// Downsample
	{
		PIXMarker(pCommandContext->GetD3DCommandList(), L"Bloom Downsample");

		auto pOutput1 = ResourceRegistry.GetTexture(Resources[EResources::BloomRenderTarget2a]);
		auto pOutput2 = ResourceRegistry.GetTexture(Resources[EResources::BloomRenderTarget3a]);
		auto pOutput3 = ResourceRegistry.GetTexture(Resources[EResources::BloomRenderTarget4a]);
		auto pOutput4 = ResourceRegistry.GetTexture(Resources[EResources::BloomRenderTarget5a]);

		size_t BloomSRVIndex = ResourceRegistry.GetShaderResourceDescriptorIndex(Resources[EResources::BloomRenderTarget1a]);
		size_t Output1UAVIndex = ResourceRegistry.GetUnorderedAccessDescriptorIndex(Resources[EResources::BloomRenderTarget2a]);
		size_t Output2UAVIndex = ResourceRegistry.GetUnorderedAccessDescriptorIndex(Resources[EResources::BloomRenderTarget3a]);
		size_t Output3UAVIndex = ResourceRegistry.GetUnorderedAccessDescriptorIndex(Resources[EResources::BloomRenderTarget4a]);
		size_t Output4UAVIndex = ResourceRegistry.GetUnorderedAccessDescriptorIndex(Resources[EResources::BloomRenderTarget5a]);

		pCommandContext->TransitionBarrier(pOutput1, Resource::State::UnorderedAccess);
		pCommandContext->TransitionBarrier(pOutput2, Resource::State::UnorderedAccess);
		pCommandContext->TransitionBarrier(pOutput3, Resource::State::UnorderedAccess);
		pCommandContext->TransitionBarrier(pOutput4, Resource::State::UnorderedAccess);

		pCommandContext->SetPipelineState(ResourceRegistry.GetComputePSO(ComputePSOs::PostProcess_BloomDownsample));
		pCommandContext->SetComputeRootSignature(ResourceRegistry.GetRootSignature(RootSignatures::PostProcess_BloomDownsample));

		struct
		{
			DirectX::XMFLOAT2 InverseOutputSize;
		} DownsampleSettings;
		DownsampleSettings.InverseOutputSize = { 1.0f / bloomWidth, 1.0f / bloomHeight };
		pCommandContext->SetComputeRoot32BitConstants(0, 2, &DownsampleSettings, 0);
		pCommandContext->SetComputeRootDescriptorTable(1, ResourceRegistry.ShaderResourceViews[BloomSRVIndex].GPUHandle);
		pCommandContext->SetComputeRootDescriptorTable(2, ResourceRegistry.UnorderedAccessViews[Output1UAVIndex].GPUHandle);
		pCommandContext->SetComputeRootDescriptorTable(3, ResourceRegistry.UnorderedAccessViews[Output2UAVIndex].GPUHandle);
		pCommandContext->SetComputeRootDescriptorTable(4, ResourceRegistry.UnorderedAccessViews[Output3UAVIndex].GPUHandle);
		pCommandContext->SetComputeRootDescriptorTable(5, ResourceRegistry.UnorderedAccessViews[Output4UAVIndex].GPUHandle);

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
		Blur(EResources::BloomRenderTarget5a, EResources::BloomRenderTarget5b, ResourceRegistry, pCommandContext);
		UpsampleBlur(EResources::BloomRenderTarget4a, EResources::BloomRenderTarget5b, EResources::BloomRenderTarget4b, ResourceRegistry, pCommandContext);
		UpsampleBlur(EResources::BloomRenderTarget3a, EResources::BloomRenderTarget4b, EResources::BloomRenderTarget3b, ResourceRegistry, pCommandContext);
		UpsampleBlur(EResources::BloomRenderTarget2a, EResources::BloomRenderTarget3b, EResources::BloomRenderTarget2b, ResourceRegistry, pCommandContext);
		UpsampleBlur(EResources::BloomRenderTarget1a, EResources::BloomRenderTarget2b, EResources::BloomRenderTarget1b, ResourceRegistry, pCommandContext);
	}

	// Apply bloom
	{
		auto pAccumulationRenderPass = ResourceRegistry.GetRenderPass<Accumulation>();
		size_t InputSRVIndex = ResourceRegistry.GetShaderResourceDescriptorIndex(pAccumulationRenderPass->Resources[Accumulation::EResources::RenderTarget]);
		auto InputSRV = ResourceRegistry.ShaderResourceViews[InputSRVIndex];

		auto pBloom = ResourceRegistry.GetTexture(Resources[EResources::BloomRenderTarget1b]);
		auto pOutput = ResourceRegistry.GetTexture(Resources[EResources::RenderTarget]);
		
		size_t BloomSRVIndex = ResourceRegistry.GetShaderResourceDescriptorIndex(Resources[EResources::BloomRenderTarget1b]);
		size_t OutputUAVIndex = ResourceRegistry.GetUnorderedAccessDescriptorIndex(Resources[EResources::RenderTarget]);

		pCommandContext->TransitionBarrier(pOutput, Resource::State::UnorderedAccess);

		pCommandContext->SetPipelineState(ResourceRegistry.GetComputePSO(ComputePSOs::PostProcess_BloomComposition));
		pCommandContext->SetComputeRootSignature(ResourceRegistry.GetRootSignature(RootSignatures::PostProcess_BloomComposition));

		struct
		{
			DirectX::XMFLOAT2 InverseOutputSize;
			float Intensity;
		} BloomSettings;
		BloomSettings.InverseOutputSize = { 1.0f / pOutput->GetWidth(), 1.0f / pOutput->GetHeight() };
		BloomSettings.Intensity = Settings.Bloom.Intensity;
		pCommandContext->SetComputeRoot32BitConstants(0, 3, &BloomSettings, 0);
		pCommandContext->SetComputeRootDescriptorTable(1, InputSRV.GPUHandle);
		pCommandContext->SetComputeRootDescriptorTable(2, ResourceRegistry.ShaderResourceViews[BloomSRVIndex].GPUHandle);
		pCommandContext->SetComputeRootDescriptorTable(3, ResourceRegistry.UnorderedAccessViews[OutputUAVIndex].GPUHandle);

		UINT threadGroupCountX = Math::RoundUpAndDivide(pOutput->GetWidth(), 8);
		UINT threadGroupCountY = Math::RoundUpAndDivide(pOutput->GetHeight(), 8);
		pCommandContext->Dispatch(threadGroupCountX, threadGroupCountY, 1);

		pCommandContext->TransitionBarrier(pOutput, Resource::State::PixelShaderResource);
	}
}

void PostProcess::ApplyTonemappingToSwapChain(ResourceRegistry& ResourceRegistry, CommandContext* pCommandContext)
{
	PIXMarker(pCommandContext->GetD3DCommandList(), L"Tonemap");

	auto pAccumulationRenderPass = ResourceRegistry.GetRenderPass<Accumulation>();
	size_t InputSRVIndex = Settings.ApplyBloom ? ResourceRegistry.GetShaderResourceDescriptorIndex(Resources[EResources::RenderTarget]) : 
		ResourceRegistry.GetShaderResourceDescriptorIndex(pAccumulationRenderPass->Resources[Accumulation::EResources::RenderTarget]);
	auto InputSRV = ResourceRegistry.ShaderResourceViews[InputSRVIndex];
	auto pDestination = ResourceRegistry.GetCurrentSwapChainBuffer();
	auto DestinationRTV = ResourceRegistry.GetCurrentSwapChainRTV();

	pCommandContext->TransitionBarrier(pDestination, Resource::State::RenderTarget);

	pCommandContext->SetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
	pCommandContext->SetPipelineState(ResourceRegistry.GetGraphicsPSO(GraphicsPSOs::PostProcess_Tonemapping));
	pCommandContext->SetGraphicsRootSignature(ResourceRegistry.GetRootSignature(RootSignatures::PostProcess_Tonemapping));

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