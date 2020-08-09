#pragma once
#include "Graphics/Scene/Scene.h"
#include "Graphics/RenderGraph.h"
#include "Graphics/RendererRegistry.h"

#include "Graphics/GpuBufferAllocator.h"
#include "Graphics/GpuTextureAllocator.h"

#include "ForwardRendering.h"

class RenderGraph;

struct SkyboxRenderPassData
{
	GpuBufferAllocator* pGpuBufferAllocator;
	GpuTextureAllocator* pGpuTextureAllocator;
	Buffer* pRenderPassConstantBuffer;
};

void AddSkyboxRenderPass(
	GpuBufferAllocator* pGpuBufferAllocator,
	GpuTextureAllocator* pGpuTextureAllocator,
	Buffer* pRenderPassConstantBuffer,
	RenderGraph* pRenderGraph)
{
	auto pForwardRenderingRenderPass = pRenderGraph->GetRenderPass<ForwardRenderingRenderPassData>();

	pRenderGraph->AddRenderPass<SkyboxRenderPassData>(
		RenderPassType::Graphics,
		[=](RenderPass<SkyboxRenderPassData>& This, RenderDevice* pRenderDevice)
	{
		This.Outputs = pForwardRenderingRenderPass->Outputs;
		This.ResourceViews = pForwardRenderingRenderPass->ResourceViews;

		This.Data.pGpuBufferAllocator = pGpuBufferAllocator;
		This.Data.pGpuTextureAllocator = pGpuTextureAllocator;
		This.Data.pRenderPassConstantBuffer = pRenderPassConstantBuffer;
		return [](const RenderPass<SkyboxRenderPassData>& This, const Scene& Scene, RenderGraphRegistry& RenderGraphRegistry, CommandContext* pCommandContext)
		{
			PIXMarker(pCommandContext->GetD3DCommandList(), L"Skybox");
			auto pRenderTarget = RenderGraphRegistry.GetTexture(This.Outputs[ForwardRenderingRenderPassData::Outputs::RenderTarget]);
			auto pDepthStencil = RenderGraphRegistry.GetTexture(This.Outputs[ForwardRenderingRenderPassData::Outputs::DepthStencil]);
			const auto& RenderTargetView = This.ResourceViews[ForwardRenderingRenderPassData::Outputs::RenderTarget];
			const auto& DepthStencilView = This.ResourceViews[ForwardRenderingRenderPassData::Outputs::DepthStencil];

			pCommandContext->TransitionBarrier(pRenderTarget, Resource::State::RenderTarget);
			pCommandContext->TransitionBarrier(pDepthStencil, Resource::State::DepthWrite);

			pCommandContext->SetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
			pCommandContext->SetPipelineState(RenderGraphRegistry.GetGraphicsPSO(GraphicsPSOs::Skybox));
			pCommandContext->SetGraphicsRootSignature(RenderGraphRegistry.GetRootSignature(RootSignatures::Skybox));

			This.Data.pGpuBufferAllocator->Bind(pCommandContext);
			pCommandContext->SetGraphicsRootConstantBufferView(RootParameters::StandardShaderLayout::RenderPassDataCB, This.Data.pRenderPassConstantBuffer->GetGpuVirtualAddressAt(NUM_CASCADES));
			pCommandContext->SetGraphicsRootDescriptorTable(RootParameters::StandardShaderLayout::DescriptorTables, RenderGraphRegistry.GetUniversalGpuDescriptorHeapSRVDescriptorHandleFromStart());

			D3D12_VIEWPORT vp;
			vp.TopLeftX = vp.TopLeftY = 0.0f;
			vp.MinDepth = 0.0f;
			vp.MaxDepth = 1.0f;
			vp.Width = pRenderTarget->GetWidth();
			vp.Height = pRenderTarget->GetHeight();

			D3D12_RECT sr;
			sr.left = sr.top = 0;
			sr.right = pRenderTarget->GetWidth();
			sr.bottom = pRenderTarget->GetHeight();

			pCommandContext->SetViewports(1, &vp);
			pCommandContext->SetScissorRects(1, &sr);

			pCommandContext->SetRenderTargets(1, RenderTargetView[0], TRUE, DepthStencilView[0]);

			pCommandContext->DrawIndexedInstanced(Scene.Skybox.Mesh.IndexCount, 1, Scene.Skybox.Mesh.StartIndexLocation, Scene.Skybox.Mesh.BaseVertexLocation, 0);

			pCommandContext->TransitionBarrier(pRenderTarget, Resource::State::PixelShaderResource);
			pCommandContext->TransitionBarrier(pDepthStencil, Resource::State::DepthRead);
		};
	},
		[=](RenderPass<SkyboxRenderPassData>& This, UINT Width, UINT Height, RenderDevice* pRenderDevice)
	{
		This.Outputs = pForwardRenderingRenderPass->Outputs;
		This.ResourceViews = pForwardRenderingRenderPass->ResourceViews;
	});
}