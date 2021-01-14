#include "pch.h"
#include "PostProcess.h"

#include "Graphics/Scene/Scene.h"
#include "Graphics/GpuScene.h"
#include "Graphics/RenderGraph.h"
#include "Graphics/RendererRegistry.h"

#include "Accumulation.h"

constexpr size_t NumTonemapRootConstants = 1 + sizeof(PostProcess::GranTurismoOperator) / 4;

PostProcess::PostProcess(UINT Width, UINT Height)
	: RenderPass("Post Process", { Width, Height }, NumResources)
{
	ExplicitResourceTransition = true;
}

void PostProcess::InitializePipeline(RenderDevice* pRenderDevice)
{
	Resources[RenderTarget]			= pRenderDevice->InitializeRenderResourceHandle(RenderResourceType::Texture, "Render Pass[" + Name + "]: " + "RenderTarget");
	Resources[BloomRenderTarget1a]	= pRenderDevice->InitializeRenderResourceHandle(RenderResourceType::Texture, "Render Pass[" + Name + "]: " + "BloomRenderTarget1a");
	Resources[BloomRenderTarget1b]	= pRenderDevice->InitializeRenderResourceHandle(RenderResourceType::Texture, "Render Pass[" + Name + "]: " + "BloomRenderTarget1b");
	Resources[BloomRenderTarget2a]	= pRenderDevice->InitializeRenderResourceHandle(RenderResourceType::Texture, "Render Pass[" + Name + "]: " + "BloomRenderTarget2a");
	Resources[BloomRenderTarget2b]	= pRenderDevice->InitializeRenderResourceHandle(RenderResourceType::Texture, "Render Pass[" + Name + "]: " + "BloomRenderTarget2b");
	Resources[BloomRenderTarget3a]	= pRenderDevice->InitializeRenderResourceHandle(RenderResourceType::Texture, "Render Pass[" + Name + "]: " + "BloomRenderTarget3a");
	Resources[BloomRenderTarget3b]	= pRenderDevice->InitializeRenderResourceHandle(RenderResourceType::Texture, "Render Pass[" + Name + "]: " + "BloomRenderTarget3b");
	Resources[BloomRenderTarget4a]	= pRenderDevice->InitializeRenderResourceHandle(RenderResourceType::Texture, "Render Pass[" + Name + "]: " + "BloomRenderTarget4a");
	Resources[BloomRenderTarget4b]	= pRenderDevice->InitializeRenderResourceHandle(RenderResourceType::Texture, "Render Pass[" + Name + "]: " + "BloomRenderTarget4b");
	Resources[BloomRenderTarget5a]	= pRenderDevice->InitializeRenderResourceHandle(RenderResourceType::Texture, "Render Pass[" + Name + "]: " + "BloomRenderTarget5a");
	Resources[BloomRenderTarget5b]	= pRenderDevice->InitializeRenderResourceHandle(RenderResourceType::Texture, "Render Pass[" + Name + "]: " + "BloomRenderTarget5b");

	pRenderDevice->CreateRootSignature(RootSignatures::PostProcess_Tonemapping, [&](RootSignatureBuilder& Builder)
	{
		Builder.AddRootConstantsParameter(RootConstants<void>(0, 0, NumTonemapRootConstants)); // Settings b0 | space0

		Builder.DenyTessellationShaderAccess();
		Builder.DenyGSAccess();
	});

	pRenderDevice->CreateRootSignature(RootSignatures::PostProcess_BloomMask, [](RootSignatureBuilder& Builder)
	{
		Builder.AddRootConstantsParameter(RootConstants<void>(0, 0, 5));

		Builder.AddStaticSampler(0, D3D12_FILTER_MIN_MAG_MIP_LINEAR, D3D12_TEXTURE_ADDRESS_MODE_CLAMP, 0);

		Builder.DenyTessellationShaderAccess();
		Builder.DenyGSAccess();
	});

	pRenderDevice->CreateRootSignature(RootSignatures::PostProcess_BloomDownsample, [](RootSignatureBuilder& Builder)
	{
		Builder.AddRootConstantsParameter(RootConstants<void>(0, 0, 7));

		Builder.AddStaticSampler(0, D3D12_FILTER_MIN_MAG_MIP_LINEAR, D3D12_TEXTURE_ADDRESS_MODE_CLAMP, 0);

		Builder.DenyTessellationShaderAccess();
		Builder.DenyGSAccess();
	});

	pRenderDevice->CreateRootSignature(RootSignatures::PostProcess_BloomBlur, [](RootSignatureBuilder& Builder)
	{
		Builder.AddRootConstantsParameter(RootConstants<void>(0, 0, 4));

		Builder.AllowInputLayout();
		Builder.DenyTessellationShaderAccess();
		Builder.DenyGSAccess();
	});

	pRenderDevice->CreateRootSignature(RootSignatures::PostProcess_BloomUpsampleBlurAccumulation, [](RootSignatureBuilder& Builder)
	{
		Builder.AddRootConstantsParameter(RootConstants<void>(0, 0, 6));

		Builder.AddStaticSampler(0, D3D12_FILTER_MIN_MAG_MIP_LINEAR, D3D12_TEXTURE_ADDRESS_MODE_BORDER, 0, D3D12_COMPARISON_FUNC_LESS_EQUAL, D3D12_STATIC_BORDER_COLOR_OPAQUE_BLACK);

		Builder.DenyTessellationShaderAccess();
		Builder.DenyGSAccess();
	});

	pRenderDevice->CreateRootSignature(RootSignatures::PostProcess_BloomComposition, [](RootSignatureBuilder& Builder)
	{
		Builder.AddRootConstantsParameter(RootConstants<void>(0, 0, 6));

		Builder.AddStaticSampler(0, D3D12_FILTER_MIN_MAG_MIP_LINEAR, D3D12_TEXTURE_ADDRESS_MODE_CLAMP, 0);

		Builder.DenyTessellationShaderAccess();
		Builder.DenyGSAccess();
	});

	pRenderDevice->CreateGraphicsPipelineState(GraphicsPSOs::PostProcess_Tonemapping, [=](GraphicsPipelineStateBuilder& Builder)
	{
		Builder.pRootSignature = pRenderDevice->GetRootSignature(RootSignatures::PostProcess_Tonemapping);
		Builder.pVS = &Shaders::VS::Quad;
		Builder.pPS = &Shaders::PS::PostProcess_Tonemapping;

		Builder.DepthStencilState.SetDepthEnable(false);

		Builder.PrimitiveTopology = PrimitiveTopology::Triangle;
		Builder.AddRenderTargetFormat(RenderDevice::SwapChainBufferFormat);
	});

	pRenderDevice->CreateComputePipelineState(ComputePSOs::PostProcess_BloomMask, [=](ComputePipelineStateBuilder& Builder)
	{
		Builder.pRootSignature = pRenderDevice->GetRootSignature(RootSignatures::PostProcess_BloomMask);
		Builder.pCS = &Shaders::CS::PostProcess_BloomMask;
	});

	pRenderDevice->CreateComputePipelineState(ComputePSOs::PostProcess_BloomDownsample, [=](ComputePipelineStateBuilder& Builder)
	{
		Builder.pRootSignature = pRenderDevice->GetRootSignature(RootSignatures::PostProcess_BloomDownsample);
		Builder.pCS = &Shaders::CS::PostProcess_BloomDownsample;
	});

	pRenderDevice->CreateComputePipelineState(ComputePSOs::PostProcess_BloomBlur, [=](ComputePipelineStateBuilder& Builder)
	{
		Builder.pRootSignature = pRenderDevice->GetRootSignature(RootSignatures::PostProcess_BloomBlur);
		Builder.pCS = &Shaders::CS::PostProcess_BloomBlur;
	});

	pRenderDevice->CreateComputePipelineState(ComputePSOs::PostProcess_BloomUpsampleBlurAccumulation, [=](ComputePipelineStateBuilder& Builder)
	{
		Builder.pRootSignature = pRenderDevice->GetRootSignature(RootSignatures::PostProcess_BloomUpsampleBlurAccumulation);
		Builder.pCS = &Shaders::CS::PostProcess_BloomUpsampleBlurAccumulation;
	});

	pRenderDevice->CreateComputePipelineState(ComputePSOs::PostProcess_BloomComposition, [=](ComputePipelineStateBuilder& Builder)
	{
		Builder.pRootSignature = pRenderDevice->GetRootSignature(RootSignatures::PostProcess_BloomComposition);
		Builder.pCS = &Shaders::CS::PostProcess_BloomComposition;
	});
}

void PostProcess::ScheduleResource(ResourceScheduler* pResourceScheduler, RenderGraph* pRenderGraph)
{
	pResourceScheduler->AllocateTexture(Resource::Type::Texture2D, [&](TextureProxy& proxy)
	{
		proxy.SetFormat(DXGI_FORMAT_R32G32B32A32_FLOAT);
		proxy.SetWidth(Properties.Width);
		proxy.SetHeight(Properties.Height);
		proxy.BindFlags = Resource::Flags::UnorderedAccess;
		proxy.InitialState = Resource::State::UnorderedAccess;
	});

	UINT denominatorIndex = 0;
	const UINT denominators[EResources::NumResources / 2] = { 1, 2, 4, 8, 16 };
	for (size_t i = 1; i < EResources::NumResources; i += 2, denominatorIndex++)
	{
		pResourceScheduler->AllocateTexture(Resource::Type::Texture2D, [&](TextureProxy& proxy)
		{
			proxy.SetFormat(DXGI_FORMAT_R32G32B32A32_FLOAT);
			proxy.SetWidth(Properties.Width / denominators[denominatorIndex]);
			proxy.SetHeight(Properties.Height / denominators[denominatorIndex]);
			proxy.BindFlags = Resource::Flags::UnorderedAccess;
			proxy.InitialState = Resource::State::UnorderedAccess;
		});

		pResourceScheduler->AllocateTexture(Resource::Type::Texture2D, [&](TextureProxy& proxy)
		{
			proxy.SetFormat(DXGI_FORMAT_R32G32B32A32_FLOAT);
			proxy.SetWidth(Properties.Width / denominators[denominatorIndex]);
			proxy.SetHeight(Properties.Height / denominators[denominatorIndex]);
			proxy.BindFlags = Resource::Flags::UnorderedAccess;
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
			settings = Settings();
		}

		ImGui::Checkbox("Apply Bloom", &settings.ApplyBloom);
		if (ImGui::TreeNode("Bloom"))
		{
			ImGui::SliderFloat("Threshold", &settings.Bloom.Threshold, 0.0f, 8.0f);
			ImGui::SliderFloat("Intensity", &settings.Bloom.Intensity, 0.0f, 2.0f);
			ImGui::TreePop();
		}

		ImGui::TreePop();
	}
}

void PostProcess::Execute(RenderContext& RenderContext, RenderGraph* pRenderGraph)
{
	auto pAccumulationRenderPass = pRenderGraph->GetRenderPass<Accumulation>();
	Descriptor InputSRV = RenderContext.GetShaderResourceView(pAccumulationRenderPass->Resources[Accumulation::EResources::RenderTarget]);

	if (settings.ApplyBloom)
		ApplyBloom(InputSRV, RenderContext, pRenderGraph);
	ApplyTonemapAndGuiToSwapChain(InputSRV, RenderContext, pRenderGraph);
}

void PostProcess::StateRefresh()
{

}

void PostProcess::ApplyBloom(Descriptor InputSRV, RenderContext& RenderContext, RenderGraph* pRenderGraph)
{
	auto& CommandList = RenderContext.GetCommandList();
	/*
		Bloom PP is based on Microsoft's DirectX-Graphics-Samples's MiniEngine.
		https://github.com/microsoft/DirectX-Graphics-Samples/tree/master/MiniEngine
	*/
	PIXScopedEvent(CommandList.GetApiHandle(), 0, L"Bloom");

	// Bloom Mask
	{
		PIXScopedEvent(CommandList.GetApiHandle(), 0, L"Bloom Mask");

		auto pOutput = RenderContext.GetTexture(Resources[EResources::BloomRenderTarget1a])->GetApiHandle();

		struct BloomMask_SubPass_Data
		{
			float2 InverseOutputSize;
			float Threshold;
			uint InputIndex;
			uint OutputIndex;
		} data = {};
		data.InverseOutputSize = { 1.0f / Properties.Width, 1.0f / Properties.Height };
		data.Threshold = settings.Bloom.Threshold;
		data.InputIndex = InputSRV.HeapIndex;
		data.OutputIndex = RenderContext.GetUnorderedAccessView(Resources[EResources::BloomRenderTarget1a]).HeapIndex;

		CommandList.TransitionBarrier(pOutput, Resource::State::UnorderedAccess);

		RenderContext.SetPipelineState(ComputePSOs::PostProcess_BloomMask);
		RenderContext.SetRoot32BitConstants(0, 5, &data, 0);

		CommandList.Dispatch2D<8, 8>(Properties.Width, Properties.Height);

		CommandList.TransitionBarrier(pOutput, Resource::State::NonPixelShaderResource);
	}

	// Bloom Downsample
	{
		PIXScopedEvent(CommandList.GetApiHandle(), 0, L"Bloom Downsample");

		auto pOutput1 = RenderContext.GetTexture(Resources[EResources::BloomRenderTarget2a]);

		struct BloomDownsample_SubPass_Data
		{
			float2 InverseOutputSize;
			uint BloomIndex;
			uint Output1Index;
			uint Output2Index;
			uint Output3Index;
			uint Output4Index;
		} data = {};

		data.InverseOutputSize = { 1.0f / Properties.Width, 1.0f / Properties.Height };
		data.BloomIndex = RenderContext.GetShaderResourceView(Resources[EResources::BloomRenderTarget1a]).HeapIndex;
		data.Output1Index = RenderContext.GetUnorderedAccessView(Resources[EResources::BloomRenderTarget2a]).HeapIndex;
		data.Output2Index = RenderContext.GetUnorderedAccessView(Resources[EResources::BloomRenderTarget3a]).HeapIndex;
		data.Output3Index = RenderContext.GetUnorderedAccessView(Resources[EResources::BloomRenderTarget4a]).HeapIndex;
		data.Output4Index = RenderContext.GetUnorderedAccessView(Resources[EResources::BloomRenderTarget5a]).HeapIndex;

		RenderContext.TransitionBarrier(Resources[EResources::BloomRenderTarget2a], Resource::State::UnorderedAccess);
		RenderContext.TransitionBarrier(Resources[EResources::BloomRenderTarget3a], Resource::State::UnorderedAccess);
		RenderContext.TransitionBarrier(Resources[EResources::BloomRenderTarget4a], Resource::State::UnorderedAccess);
		RenderContext.TransitionBarrier(Resources[EResources::BloomRenderTarget5a], Resource::State::UnorderedAccess);

		RenderContext.SetPipelineState(ComputePSOs::PostProcess_BloomDownsample);
		RenderContext.SetRoot32BitConstants(0, 7, &data, 0);

		CommandList.Dispatch2D<8, 8>(Properties.Width / 2, Properties.Height / 2);

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
		PIXScopedEvent(CommandList.GetApiHandle(), 0, L"Bloom Composition");

		auto pBloom = RenderContext.GetTexture(Resources[EResources::BloomRenderTarget1b]);
		auto pOutput = RenderContext.GetTexture(Resources[EResources::RenderTarget]);

		struct BloomComposition_SubPass_Data
		{
			float2 InverseOutputSize;
			float Intensity;
			uint InputIndex;
			uint BloomIndex;
			uint OutputIndex;
		} data = {};
		data.InverseOutputSize = { 1.0f / pOutput->Width, 1.0f / pOutput->Height };
		data.Intensity = settings.Bloom.Intensity;
		data.InputIndex = InputSRV.HeapIndex;
		data.BloomIndex = RenderContext.GetShaderResourceView(Resources[EResources::BloomRenderTarget1b]).HeapIndex;
		data.OutputIndex = RenderContext.GetUnorderedAccessView(Resources[EResources::RenderTarget]).HeapIndex;

		RenderContext.TransitionBarrier(Resources[EResources::RenderTarget], Resource::State::UnorderedAccess);

		RenderContext.SetPipelineState(ComputePSOs::PostProcess_BloomComposition);
		RenderContext.SetRoot32BitConstants(0, 6, &data, 0);

		CommandList.Dispatch2D<8, 8>(pOutput->Width, pOutput->Height);

		RenderContext.TransitionBarrier(Resources[EResources::RenderTarget], Resource::State::PixelShaderResource);
	}
}

void PostProcess::ApplyTonemapAndGuiToSwapChain(Descriptor InputSRV, RenderContext& RenderContext, RenderGraph* pRenderGraph)
{
	auto& CommandList = RenderContext.GetCommandList();

	auto pDestination = RenderContext.GetTexture(RenderContext.GetCurrentBackBufferHandle());
	auto DestinationRTV = RenderContext.GetRenderTargetView(RenderContext.GetCurrentBackBufferHandle()).CpuHandle;

	CommandList.TransitionBarrier(pDestination->GetApiHandle(), Resource::State::RenderTarget);
	{
		D3D12_VIEWPORT	vp = CD3DX12_VIEWPORT(0.0f, 0.0f, pDestination->Width, pDestination->Height);
		D3D12_RECT		sr = CD3DX12_RECT(0, 0, pDestination->Width, pDestination->Height);

		CommandList->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
		CommandList->RSSetViewports(1, &vp);
		CommandList->RSSetScissorRects(1, &sr);
		CommandList->OMSetRenderTargets(1, &DestinationRTV, TRUE, nullptr);

		// Tonemap
		{
			PIXScopedEvent(CommandList.GetApiHandle(), 0, L"Tonemap");

			struct Tonemapping_SubPass_Data
			{
				uint InputIndex;

				float MaximumBrightness;
				float Contrast;
				float LinearSectionStart;
				float LinearSectionLength;
				float BlackTightness_C;
				float BlackTightness_B;
			} data = {};
			data.InputIndex				= settings.ApplyBloom ? RenderContext.GetShaderResourceView(Resources[EResources::RenderTarget]).HeapIndex :
				InputSRV.HeapIndex;
			data.MaximumBrightness		= granTurismoOperator.MaximumBrightness;
			data.Contrast				= granTurismoOperator.Contrast;
			data.LinearSectionStart		= granTurismoOperator.LinearSectionStart;
			data.LinearSectionLength	= granTurismoOperator.LinearSectionLength;
			data.BlackTightness_C		= granTurismoOperator.BlackTightness_C;
			data.BlackTightness_B		= granTurismoOperator.BlackTightness_B;

			RenderContext.SetPipelineState(GraphicsPSOs::PostProcess_Tonemapping);
			RenderContext.SetRoot32BitConstants(0, NumTonemapRootConstants, &data, 0);

			CommandList.DrawInstanced(3, 1, 0, 0);
		}

		// ImGui Render
		{
			PIXScopedEvent(CommandList.GetApiHandle(), 0, L"ImGui Render");

			ImGui::Render();
			ImGui_ImplDX12_RenderDrawData(ImGui::GetDrawData(), CommandList);
		}
	}
	CommandList.TransitionBarrier(pDestination->GetApiHandle(), Resource::State::Present);
}

void PostProcess::Blur(size_t Input, size_t Output, RenderContext& RenderContext)
{
	auto& CommandList = RenderContext.GetCommandList();
	PIXScopedEvent(CommandList.GetApiHandle(), 0, L"Bloom Blur");

	auto pOutput = RenderContext.GetTexture(Resources[Output]);

	struct BloomBlur_SubPass_Data
	{
		float2 InverseOutputSize;
		uint InputIndex;
		uint OutputIndex;
	} data = {};
	data.InverseOutputSize = { 1.0f / pOutput->Width, 1.0f / pOutput->Height };
	data.InputIndex = RenderContext.GetShaderResourceView(Resources[Input]).HeapIndex;
	data.OutputIndex = RenderContext.GetUnorderedAccessView(Resources[Output]).HeapIndex;

	RenderContext.TransitionBarrier(Resources[Input], Resource::State::NonPixelShaderResource);
	RenderContext.TransitionBarrier(Resources[Output], Resource::State::UnorderedAccess);

	RenderContext.SetPipelineState(ComputePSOs::PostProcess_BloomBlur);
	RenderContext.SetRoot32BitConstants(0, 4, &data, 0);

	CommandList.Dispatch2D<8, 8>(pOutput->Width, pOutput->Height);

	RenderContext.TransitionBarrier(Resources[Output], Resource::State::NonPixelShaderResource);
}

void PostProcess::UpsampleBlurAccumulation(size_t HighResolution, size_t LowResolution, size_t Output, RenderContext& RenderContext)
{
	auto& CommandList = RenderContext.GetCommandList();
	PIXScopedEvent(CommandList.GetApiHandle(), 0, L"Bloom Upsample Blur Accumulation");

	auto pOutput = RenderContext.GetTexture(Resources[Output]);

	struct BloomUpsampleBlurAccumulation_SubPass_Data
	{
		float2 InverseOutputSize;
		float UpsampleInterpolationFactor;
		uint HighResolutionIndex;
		uint LowResolutionIndex;
		uint OutputIndex;
	} data = {};
	data.InverseOutputSize = { 1.0f / pOutput->Width, 1.0f / pOutput->Height };
	data.UpsampleInterpolationFactor = 0.5f;
	data.HighResolutionIndex = RenderContext.GetShaderResourceView(Resources[HighResolution]).HeapIndex;
	data.LowResolutionIndex = RenderContext.GetShaderResourceView(Resources[LowResolution]).HeapIndex;
	data.OutputIndex = RenderContext.GetUnorderedAccessView(Resources[Output]).HeapIndex;

	RenderContext.TransitionBarrier(Resources[HighResolution], Resource::State::NonPixelShaderResource);
	RenderContext.TransitionBarrier(Resources[LowResolution], Resource::State::NonPixelShaderResource);
	RenderContext.TransitionBarrier(Resources[Output], Resource::State::UnorderedAccess);

	RenderContext.SetPipelineState(ComputePSOs::PostProcess_BloomUpsampleBlurAccumulation);
	RenderContext.SetRoot32BitConstants(0, 6, &data, 0);

	CommandList.Dispatch2D<8, 8>(pOutput->Width, pOutput->Height);

	RenderContext.TransitionBarrier(Resources[Output], Resource::State::NonPixelShaderResource);
}