#pragma once
#include "Graphics/Scene/Scene.h"
#include "Graphics/RenderGraph.h"
#include "Graphics/RendererRegistry.h"
#include "Graphics/Renderer.h"

#include "ForwardRendering.h"
#include "Raytracing.h"

class RenderGraph;

struct TonemapRenderPassData
{
	TonemapData TonemapData;

	Texture* pSource;

	Texture* pDestination;		// Set in Renderer::Render
	Descriptor DestinationRTV;	// Set in Renderer::Render
};

void AddTonemapRenderPass(
	UINT InitialSwapChainBufferIndex,
	RenderGraph* pRenderGraph)
{
	constexpr bool useRasterization = Renderer::Settings::Rasterization;

	pRenderGraph->AddRenderPass<TonemapRenderPassData>(
		RenderPassType::Graphics,
		[=](RenderPass<TonemapRenderPassData>& This, RenderDevice* pRenderDevice)
	{
		if (useRasterization)
		{
			auto pForwardRenderingRenderPass = pRenderGraph->GetRenderPass<ForwardRenderingRenderPassData>();
			This.Data.pSource = pRenderDevice->GetTexture(pForwardRenderingRenderPass->Outputs[ForwardRenderingRenderPassData::Outputs::RenderTarget]);
		}
		else
		{
			auto pRaytracingRenderPass = pRenderGraph->GetRenderPass<RaytracingRenderPassData>();
			This.Data.pSource = pRenderDevice->GetTexture(pRaytracingRenderPass->Outputs[RaytracingRenderPassData::Outputs::RenderTarget]);
		}

		return [](const RenderPass<TonemapRenderPassData>& This, const Scene& Scene, RenderGraphRegistry& RenderGraphRegistry, CommandContext* pCommandContext)
		{
			PIXMarker(pCommandContext->GetD3DCommandList(), L"Tonemap");
			auto pOutput = This.Data.pDestination;

			pCommandContext->TransitionBarrier(pOutput, Resource::State::RenderTarget);

			pCommandContext->SetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
			pCommandContext->SetPipelineState(RenderGraphRegistry.GetGraphicsPSO(GraphicsPSOs::PostProcess_Tonemapping));
			pCommandContext->SetGraphicsRootSignature(RenderGraphRegistry.GetRootSignature(RootSignatures::PostProcess_Tonemapping));

			pCommandContext->SetGraphicsRoot32BitConstants(RootParameters::StandardShaderLayout::ConstantDataCB, sizeof(This.Data.TonemapData) / 4, &This.Data.TonemapData, 0);
			pCommandContext->SetGraphicsRootDescriptorTable(RootParameters::StandardShaderLayout::DescriptorTables, RenderGraphRegistry.GetUniversalGpuDescriptorHeapSRVDescriptorHandleFromStart());

			D3D12_VIEWPORT vp;
			vp.TopLeftX = vp.TopLeftY = 0.0f;
			vp.MinDepth = 0.0f;
			vp.MaxDepth = 1.0f;
			vp.Width = This.Data.pSource->GetWidth();
			vp.Height = This.Data.pSource->GetHeight();

			D3D12_RECT sr;
			sr.left = sr.top = 0;
			sr.right = This.Data.pSource->GetWidth();
			sr.bottom = This.Data.pSource->GetHeight();

			pCommandContext->SetViewports(1, &vp);
			pCommandContext->SetScissorRects(1, &sr);

			pCommandContext->SetRenderTargets(1, This.Data.DestinationRTV, TRUE, Descriptor());
			pCommandContext->DrawInstanced(3, 1, 0, 0);

			pCommandContext->TransitionBarrier(This.Data.pDestination, Resource::State::Present);
		};
	},
		[=](RenderPass<TonemapRenderPassData>& This, UINT Width, UINT Height, RenderDevice* pRenderDevice)
	{
		if (useRasterization)
		{
			auto pSkyboxRenderPass = pRenderGraph->GetRenderPass<ForwardRenderingRenderPassData>();
			This.Data.pSource = pRenderDevice->GetTexture(pSkyboxRenderPass->Outputs[ForwardRenderingRenderPassData::Outputs::RenderTarget]);
		}
		else
		{
			auto pRaytracingRenderPass = pRenderGraph->GetRenderPass<RaytracingRenderPassData>();
			This.Data.pSource = pRenderDevice->GetTexture(pRaytracingRenderPass->Outputs[RaytracingRenderPassData::Outputs::RenderTarget]);
		}
	});
}