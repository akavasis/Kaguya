#include "pch.h"
#include "ToneMapper.h"

#include "RendererRegistry.h"

void ToneMapper::Create(RenderDevice* pRenderDevice)
{
	this->pRenderDevice = pRenderDevice;

	RS = pRenderDevice->CreateRootSignature([](RootSignatureBuilder& Builder)
	{
		Builder.AddRootConstantsParameter(RootConstants<void>(0, 0, 1)); // register(b0, space0)
	});

	PSO = pRenderDevice->CreateGraphicsPipelineState([=](GraphicsPipelineStateBuilder& Builder)
	{
		Builder.pRootSignature = &RS;
		Builder.pVS = &Shaders::VS::FullScreenTriangle;
		Builder.pPS = &Shaders::PS::ToneMap;

		Builder.DepthStencilState.SetDepthEnable(false);

		Builder.PrimitiveTopology = PrimitiveTopology::Triangle;
		Builder.AddRenderTargetFormat(RenderDevice::SwapChainBufferFormat);
	});
}

void ToneMapper::Apply(Descriptor ShaderResourceView, Descriptor RenderTargetView, CommandList& CommandList)
{
	PIXScopedEvent(CommandList.GetApiHandle(), 0, L"Tone Map");

	CommandList->SetPipelineState(PSO);
	CommandList->SetGraphicsRootSignature(RS);

	pRenderDevice->BindGlobalDescriptorHeap(CommandList);
	pRenderDevice->BindDescriptorTable(PipelineState::Type::Graphics, RS, CommandList);

	struct Settings
	{
		unsigned int InputIndex;
	} Settings = {};
	Settings.InputIndex = ShaderResourceView.Index;

	CommandList->SetGraphicsRoot32BitConstants(0, 1, &Settings, 0);

	CommandList->DrawInstanced(3, 1, 0, 0);
}