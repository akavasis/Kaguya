#include "pch.h"
#include "Gui.h"

#define SHOW_IMGUI_DEMO_WINDOW 0

Gui::Gui(RenderDevice* pRenderDevice)
	: DescriptorHeap(&pRenderDevice->Device, 0, 1, 0, true)
{
	// Initialize ImGui for d3d12
	ImGui_ImplDX12_Init(pRenderDevice->Device.GetD3DDevice(), 1,
		RendererFormats::SwapChainBufferFormat, DescriptorHeap.GetD3DDescriptorHeap(),
		DescriptorHeap.GetD3DDescriptorHeap()->GetCPUDescriptorHandleForHeapStart(),
		DescriptorHeap.GetD3DDescriptorHeap()->GetGPUDescriptorHandleForHeapStart());
}

Gui::~Gui()
{
	ImGui_ImplDX12_Shutdown();
}

void Gui::BeginFrame()
{
	ImGui_ImplDX12_NewFrame();
	ImGui_ImplWin32_NewFrame();
	ImGui::NewFrame();
#if SHOW_IMGUI_DEMO_WINDOW
	ImGui::ShowDemoWindow();
#endif
}

void Gui::EndFrame(Texture* pDestination, Descriptor DestinationRTV, CommandContext* pCommandContext)
{
	PIXMarker(pCommandContext->GetD3DCommandList(), L"ImGui Render");

	pCommandContext->TransitionBarrier(pDestination, Resource::State::RenderTarget);

	pCommandContext->SetRenderTargets(1, DestinationRTV, TRUE, Descriptor());
	pCommandContext->SetDescriptorHeaps(&DescriptorHeap, nullptr);
	ImGui::Render();
	ImGui_ImplDX12_RenderDrawData(ImGui::GetDrawData(), pCommandContext->GetD3DCommandList());

	pCommandContext->TransitionBarrier(pDestination, Resource::State::Present);
}