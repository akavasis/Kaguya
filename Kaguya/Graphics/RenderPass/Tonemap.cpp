#include "pch.h"
#include "Tonemap.h"

Tonemap::Tonemap()
	: IRenderPass(RenderPassType::Graphics, {})
{

}

Tonemap::~Tonemap()
{

}

void Tonemap::Setup(RenderDevice* pRenderDevice)
{

}

void Tonemap::Update()
{

}

void Tonemap::RenderGui()
{

}

void Tonemap::Execute(const Scene& Scene, RenderGraphRegistry& RenderGraphRegistry, CommandContext* pCommandContext)
{
	PIXMarker(pCommandContext->GetD3DCommandList(), L"Tonemap");
	auto pOutput = pDestination;

	pCommandContext->TransitionBarrier(pOutput, Resource::State::RenderTarget);

	pCommandContext->SetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
	pCommandContext->SetPipelineState(RenderGraphRegistry.GetGraphicsPSO(GraphicsPSOs::PostProcess_Tonemap));
	pCommandContext->SetGraphicsRootSignature(RenderGraphRegistry.GetRootSignature(RootSignatures::PostProcess_Tonemap));

	pCommandContext->SetGraphicsRoot32BitConstants(RootParameters::StandardShaderLayout::ConstantDataCB, sizeof(Settings) / 4, &Settings, 0);
	pCommandContext->SetGraphicsRootDescriptorTable(RootParameters::StandardShaderLayout::DescriptorTables, RenderGraphRegistry.GetUniversalGpuDescriptorHeapSRVDescriptorHandleFromStart());

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

void Tonemap::Resize(UINT Width, UINT Height, RenderDevice* pRenderDevice)
{

}