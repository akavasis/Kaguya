#pragma once
#include "Graphics/Scene/Scene.h"
#include "Graphics/RenderGraph.h"
#include "Graphics/RendererRegistry.h"

#include "Raytracing.h"

struct AccumulationRenderPassData
{
	struct Outputs
	{
		enum
		{
			RenderTarget
		};
	};

	struct ResourceViews
	{
		enum
		{
			RenderTarget
		};
	};

	AccumulationData AccumulationData; // Set in Renderer
	Transform LastCameraTransform;
};

void AddAccumulationRenderPass(
	UINT Width, UINT Height,
	RenderGraph* pRenderGraph)
{
	pRenderGraph->AddRenderPass<AccumulationRenderPassData>(
		RenderPassType::Graphics,
		[=](RenderPass<AccumulationRenderPassData>& This, RenderDevice* pRenderDevice)
	{
		auto accumulationOutput = pRenderDevice->CreateTexture(Resource::Type::Texture2D, [&](TextureProxy& proxy)
		{
			proxy.SetFormat(RendererFormats::HDRBufferFormat);
			proxy.SetWidth(Width);
			proxy.SetHeight(Height);
			proxy.BindFlags = Resource::BindFlags::UnorderedAccess;
			proxy.InitialState = Resource::State::UnorderedAccess;
		});

		This.Outputs.push_back(accumulationOutput);

		auto uav = pRenderDevice->DescriptorAllocator.AllocateUADescriptors(1);
		pRenderDevice->CreateUAV(accumulationOutput, uav[0], {}, {});

		This.ResourceViews.push_back(std::move(uav));

		return [](const RenderPass<AccumulationRenderPassData>& This, const Scene& Scene, RenderGraphRegistry& RenderGraphRegistry, CommandContext* pCommandContext)
		{
			PIXMarker(pCommandContext->GetD3DCommandList(), L"Accumulation");

			auto pRaytracingRenderPass = RenderGraphRegistry.GetRenderPass<RaytracingRenderPassData>();

			auto pOutput = RenderGraphRegistry.GetTexture(This.Outputs[AccumulationRenderPassData::Outputs::RenderTarget]);

			auto pAccumulationPipelineState = RenderGraphRegistry.GetComputePSO(ComputePSOs::Accumulation);
			auto pAccumulationRootSignature = RenderGraphRegistry.GetRootSignature(RootSignatures::Raytracing::Accumulation);

			pCommandContext->TransitionBarrier(pOutput, Resource::State::UnorderedAccess);

			// Bind Pipeline
			pCommandContext->SetPipelineState(pAccumulationPipelineState);
			pCommandContext->SetComputeRootSignature(pAccumulationRootSignature);

			// Bind Resources
			pCommandContext->SetComputeRoot32BitConstants(RootParameters::Accumulation::AccumulationCBuffer, sizeof(AccumulationData) / 4, &This.Data.AccumulationData, 0);
			pCommandContext->SetComputeRootDescriptorTable(RootParameters::Accumulation::Input, pRaytracingRenderPass->ResourceViews[RaytracingRenderPassData::ResourceViews::RenderTarget].GetStartDescriptor().GPUHandle);
			pCommandContext->SetComputeRootDescriptorTable(RootParameters::Accumulation::Output, This.ResourceViews[AccumulationRenderPassData::ResourceViews::RenderTarget].GetStartDescriptor().GPUHandle);

			UINT threadGroupCountX = Math::RoundUpAndDivide(pOutput->GetWidth(), 16);
			UINT threadGroupCountY = Math::RoundUpAndDivide(pOutput->GetHeight(), 16);
			pCommandContext->Dispatch(threadGroupCountX, threadGroupCountY, 1);

			pCommandContext->TransitionBarrier(pOutput, Resource::State::PixelShaderResource);
		};
	},
		[](RenderPass<AccumulationRenderPassData>& This, UINT Width, UINT Height, RenderDevice* pRenderDevice)
	{
		// Destroy render target
		for (auto& output : This.Outputs)
		{
			pRenderDevice->Destroy(&output);
		}

		// Recreate render target
		This.Outputs[AccumulationRenderPassData::Outputs::RenderTarget] = pRenderDevice->CreateTexture(Resource::Type::Texture2D, [&](TextureProxy& proxy)
		{
			proxy.SetFormat(RendererFormats::HDRBufferFormat);
			proxy.SetWidth(Width);
			proxy.SetHeight(Height);
			proxy.BindFlags = Resource::BindFlags::UnorderedAccess;
			proxy.InitialState = Resource::State::UnorderedAccess;
		});

		const auto& uav = This.ResourceViews[AccumulationRenderPassData::ResourceViews::RenderTarget];
		pRenderDevice->CreateUAV(This.Outputs[AccumulationRenderPassData::Outputs::RenderTarget], uav[0], {}, {});
	});
}