#include "pch.h"
#include "Tonemapping.h"

Tonemapping::Tonemapping(Descriptor Input)
	: IRenderPass(RenderPassType::Graphics, {}),
	Input(Input)
{

}

Tonemapping::~Tonemapping()
{

}

bool Tonemapping::Initialize(GpuScene* pGpuScene, RenderDevice* pRenderDevice)
{
	pDestination = pRenderDevice->GetTexture(pRenderDevice->SwapChainTextures[pRenderDevice->FrameIndex]);
	DestinationRTV = pRenderDevice->SwapChainRenderTargetViews[pRenderDevice->FrameIndex];
	return true;
}

void Tonemapping::Update(GpuScene* pGpuScene, RenderDevice* pRenderDevice)
{
	pDestination = pRenderDevice->GetTexture(pRenderDevice->SwapChainTextures[pRenderDevice->FrameIndex]);
	DestinationRTV = pRenderDevice->SwapChainRenderTargetViews[pRenderDevice->FrameIndex];
}

void Tonemapping::RenderGui()
{
	if (ImGui::TreeNode("Tonemap"))
	{
		if (ImGui::Button("Restore Defaults"))
		{
			Settings.Exposure = 0.5f;
		}
		ImGui::SliderFloat("Exposure", &Settings.Exposure, 0.1f, 10.0f);
		//ImGui::SliderFloat("Gamma", &Settings.Gamma, 0.1f, 4.0f);
		ImGui::TreePop();
	}
}

void Tonemapping::Execute(RenderGraphRegistry& RenderGraphRegistry, CommandContext* pCommandContext)
{
	PIXMarker(pCommandContext->GetD3DCommandList(), L"Tonemap");

	auto pOutput = pDestination;

	pCommandContext->TransitionBarrier(pOutput, Resource::State::RenderTarget);

	pCommandContext->SetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
	pCommandContext->SetPipelineState(RenderGraphRegistry.GetGraphicsPSO(GraphicsPSOs::PostProcess_Tonemapping));
	pCommandContext->SetGraphicsRootSignature(RenderGraphRegistry.GetRootSignature(RootSignatures::PostProcess_Tonemapping));

	pCommandContext->SetGraphicsRoot32BitConstants(RootParameters::Tonemapping::Settings, sizeof(Settings) / 4, &Settings, 0);
	pCommandContext->SetGraphicsRootDescriptorTable(RootParameters::Tonemapping::Input, Input.GPUHandle);

	D3D12_VIEWPORT vp;
	vp.TopLeftX = vp.TopLeftY = 0.0f;
	vp.MinDepth = 0.0f;
	vp.MaxDepth = 1.0f;
	vp.Width = pOutput->GetWidth();
	vp.Height = pOutput->GetHeight();

	D3D12_RECT sr;
	sr.left = sr.top = 0;
	sr.right = pOutput->GetWidth();
	sr.bottom = pOutput->GetHeight();

	pCommandContext->SetViewports(1, &vp);
	pCommandContext->SetScissorRects(1, &sr);

	pCommandContext->SetRenderTargets(1, DestinationRTV, TRUE, Descriptor());
	pCommandContext->DrawInstanced(3, 1, 0, 0);

	pCommandContext->TransitionBarrier(pDestination, Resource::State::Present);
}

void Tonemapping::Resize(UINT Width, UINT Height, RenderDevice* pRenderDevice)
{

}