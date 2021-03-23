#include "pch.h"
#include "ToneMapper.h"

#include "RendererRegistry.h"

void ToneMapper::Create()
{
	auto& RenderDevice = RenderDevice::Instance();

	RTV = RenderDevice.AllocateRenderTargetView();
	SRV = RenderDevice.AllocateShaderResourceView();

	RS = RenderDevice.CreateRootSignature([](RootSignatureBuilder& Builder)
	{
		Builder.AddRootConstantsParameter(RootConstants<void>(0, 0, 1)); // register(b0, space0)

		// PointClamp
		Builder.AddStaticSampler(0, D3D12_FILTER_MIN_MAG_MIP_POINT, D3D12_TEXTURE_ADDRESS_MODE_CLAMP, 16);
	});

	PSO = RenderDevice.CreateGraphicsPipelineState([=](GraphicsPipelineStateBuilder& Builder)
	{
		Builder.pRootSignature = &RS;
		Builder.pVS = &Shaders::VS::FullScreenTriangle;
		Builder.pPS = &Shaders::PS::ToneMap;

		Builder.DepthStencilState.SetDepthEnable(false);

		Builder.PrimitiveTopology = PrimitiveTopology::Triangle;
		Builder.AddRenderTargetFormat(DirectX::MakeSRGB(RenderDevice::SwapChainBufferFormat));
	});
}

void ToneMapper::SetResolution(UINT Width, UINT Height)
{
	auto& RenderDevice = RenderDevice::Instance();

	if (this->Width == Width && this->Height == Height)
	{
		return;
	}

	this->Width = Width;
	this->Height = Height;

	D3D12MA::ALLOCATION_DESC AllocationDesc = {};
	AllocationDesc.Flags = D3D12MA::ALLOCATION_FLAG_COMMITTED;
	AllocationDesc.HeapType = D3D12_HEAP_TYPE_DEFAULT;

	auto ResourceDesc = CD3DX12_RESOURCE_DESC::Tex2D(RenderDevice::SwapChainBufferFormat, Width, Height);
	ResourceDesc.MipLevels = 1;
	ResourceDesc.Flags = D3D12_RESOURCE_FLAG_ALLOW_RENDER_TARGET;

	auto ClearValue = CD3DX12_CLEAR_VALUE(DirectX::MakeSRGB(RenderDevice::SwapChainBufferFormat), DirectX::Colors::White);

	m_RenderTarget = RenderDevice.CreateResource(&AllocationDesc,
		&ResourceDesc, D3D12_RESOURCE_STATE_RENDER_TARGET, &ClearValue);

	RenderDevice.CreateRenderTargetView(m_RenderTarget->pResource.Get(), RTV, {}, {}, {}, true);
	RenderDevice.CreateShaderResourceView(m_RenderTarget->pResource.Get(), SRV);
}

void ToneMapper::Apply(Descriptor ShaderResourceView, CommandList& CommandList)
{
	auto& RenderDevice = RenderDevice::Instance();

	PIXScopedEvent(CommandList.GetApiHandle(), 0, L"Tone Map");

	CommandList->SetPipelineState(PSO);
	CommandList->SetGraphicsRootSignature(RS);

	RenderDevice.BindDescriptorTable<PipelineState::Type::Graphics>(RS, CommandList);

	auto Viewport = CD3DX12_VIEWPORT(0.0f, 0.0f, Width, Height);
	auto ScissorRect = CD3DX12_RECT(0, 0, Width, Height);

	CommandList->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
	CommandList->RSSetViewports(1, &Viewport);
	CommandList->RSSetScissorRects(1, &ScissorRect);
	CommandList->OMSetRenderTargets(1, &RTV.CpuHandle, TRUE, nullptr);
	CommandList->ClearRenderTargetView(RTV.CpuHandle, DirectX::Colors::White, 0, nullptr);

	struct Settings
	{
		unsigned int InputIndex;
	} Settings = {};
	Settings.InputIndex = ShaderResourceView.Index;

	CommandList->SetGraphicsRoot32BitConstants(0, 1, &Settings, 0);

	CommandList->DrawInstanced(3, 1, 0, 0);
}