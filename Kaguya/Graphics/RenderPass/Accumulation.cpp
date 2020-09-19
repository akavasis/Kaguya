#include "pch.h"
#include "Accumulation.h"

#include "Pathtracing.h"

Accumulation::Accumulation(UINT Width, UINT Height)
	: IRenderPass(RenderPassType::Graphics, { Width, Height, RendererFormats::HDRBufferFormat })
{
	LastAperture = 0.0f;
	LastFocalLength = 0.0f;
	LastCameraTransform = {};
}

Accumulation::~Accumulation()
{

}

void Accumulation::Setup(RenderDevice* pRenderDevice)
{
	auto accumulationOutput = pRenderDevice->CreateTexture(Resource::Type::Texture2D, [&](TextureProxy& proxy)
	{
		proxy.SetFormat(Properties.Format);
		proxy.SetWidth(Properties.Width);
		proxy.SetHeight(Properties.Height);
		proxy.BindFlags = Resource::BindFlags::UnorderedAccess;
		proxy.InitialState = Resource::State::UnorderedAccess;
	});

	Outputs.push_back(accumulationOutput);

	auto uav = pRenderDevice->DescriptorAllocator.AllocateUADescriptors(1);
	pRenderDevice->CreateUAV(accumulationOutput, uav[0], {}, {});

	ResourceViews.push_back(std::move(uav));
}

void Accumulation::Update()
{

}

void Accumulation::RenderGui()
{

}

void Accumulation::Execute(const Scene& Scene, RenderGraphRegistry& RenderGraphRegistry, CommandContext* pCommandContext)
{
	PIXMarker(pCommandContext->GetD3DCommandList(), L"Accumulation");

	auto pRaytracingRenderPass = RenderGraphRegistry.GetRenderPass<Pathtracing>();

	auto pOutput = RenderGraphRegistry.GetTexture(Outputs[EOutputs::RenderTarget]);

	auto pAccumulationPipelineState = RenderGraphRegistry.GetComputePSO(ComputePSOs::Accumulation);
	auto pAccumulationRootSignature = RenderGraphRegistry.GetRootSignature(RootSignatures::Raytracing::Accumulation);

	pCommandContext->TransitionBarrier(pOutput, Resource::State::UnorderedAccess);

	// Bind Pipeline
	pCommandContext->SetPipelineState(pAccumulationPipelineState);
	pCommandContext->SetComputeRootSignature(pAccumulationRootSignature);

	// Bind Resources
	pCommandContext->SetComputeRoot32BitConstants(RootParameters::Accumulation::AccumulationCBuffer, sizeof(Settings) / 4, &Settings, 0);
	pCommandContext->SetComputeRootDescriptorTable(RootParameters::Accumulation::Input, pRaytracingRenderPass->ResourceViews[Pathtracing::EResourceViews::RenderTarget].GetStartDescriptor().GPUHandle);
	pCommandContext->SetComputeRootDescriptorTable(RootParameters::Accumulation::Output, ResourceViews[EResourceViews::RenderTarget].GetStartDescriptor().GPUHandle);

	UINT threadGroupCountX = Math::RoundUpAndDivide(pOutput->GetWidth(), 16);
	UINT threadGroupCountY = Math::RoundUpAndDivide(pOutput->GetHeight(), 16);
	pCommandContext->Dispatch(threadGroupCountX, threadGroupCountY, 1);

	pCommandContext->TransitionBarrier(pOutput, Resource::State::PixelShaderResource);
}

void Accumulation::Resize(UINT Width, UINT Height, RenderDevice* pRenderDevice)
{
	// Destroy render target
	for (auto& output : Outputs)
	{
		pRenderDevice->Destroy(&output);
	}

	// Recreate render target
	Outputs[EOutputs::RenderTarget] = pRenderDevice->CreateTexture(Resource::Type::Texture2D, [&](TextureProxy& proxy)
	{
		proxy.SetFormat(RendererFormats::HDRBufferFormat);
		proxy.SetWidth(Width);
		proxy.SetHeight(Height);
		proxy.BindFlags = Resource::BindFlags::UnorderedAccess;
		proxy.InitialState = Resource::State::UnorderedAccess;
	});

	const auto& uav = ResourceViews[EResourceViews::RenderTarget];
	pRenderDevice->CreateUAV(Outputs[EOutputs::RenderTarget], uav[0], {}, {});
}