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

bool Accumulation::Initialize(GpuScene* pGpuScene, RenderDevice* pRenderDevice)
{
	LastAperture = pGpuScene->pScene->Camera.Aperture;
	LastFocalLength = pGpuScene->pScene->Camera.FocalLength;
	LastCameraTransform = pGpuScene->pScene->Camera.Transform;

	auto accumulationOutput = pRenderDevice->CreateTexture(Resource::Type::Texture2D, [&](TextureProxy& proxy)
	{
		proxy.SetFormat(Properties.Format);
		proxy.SetWidth(Properties.Width);
		proxy.SetHeight(Properties.Height);
		proxy.BindFlags = Resource::BindFlags::UnorderedAccess;
		proxy.InitialState = Resource::State::UnorderedAccess;
	});

	Resources.push_back(accumulationOutput);

	auto uav = pRenderDevice->DescriptorAllocator.AllocateUADescriptors(1);
	pRenderDevice->CreateUAV(accumulationOutput, uav[0], {}, {});

	auto srv = pRenderDevice->DescriptorAllocator.AllocateSRDescriptors(1);
	pRenderDevice->CreateSRV(accumulationOutput, srv[0]);

	ResourceViews.push_back(std::move(uav));
	ResourceViews.push_back(std::move(srv));

	return true;
}

void Accumulation::Update(GpuScene* pGpuScene, RenderDevice* pRenderDevice)
{
	auto pScene = pGpuScene->pScene;

	Settings.AccumulationCount++;

	// If the camera has moved
	if (pScene->Camera.Aperture != LastAperture ||
		pScene->Camera.FocalLength != LastFocalLength ||
		pScene->Camera.Transform != LastCameraTransform)
	{
		Settings.AccumulationCount = 0;
		LastAperture = pScene->Camera.Aperture;
		LastFocalLength = pScene->Camera.FocalLength;
		LastCameraTransform = pScene->Camera.Transform;
	}
}

void Accumulation::RenderGui()
{

}

void Accumulation::Execute(RenderGraphRegistry& RenderGraphRegistry, CommandContext* pCommandContext)
{
	PIXMarker(pCommandContext->GetD3DCommandList(), L"Accumulation");

	auto pPathtracingRenderPass = RenderGraphRegistry.GetRenderPass<Pathtracing>();

	auto pOutput = RenderGraphRegistry.GetTexture(Resources[EResources::RenderTarget]);

	auto pAccumulationPipelineState = RenderGraphRegistry.GetComputePSO(ComputePSOs::Accumulation);
	auto pAccumulationRootSignature = RenderGraphRegistry.GetRootSignature(RootSignatures::Raytracing::Accumulation);

	pCommandContext->TransitionBarrier(pOutput, Resource::State::UnorderedAccess);

	// Bind Pipeline
	pCommandContext->SetPipelineState(pAccumulationPipelineState);
	pCommandContext->SetComputeRootSignature(pAccumulationRootSignature);

	// Bind Resources
	pCommandContext->SetComputeRoot32BitConstants(RootParameters::Accumulation::AccumulationCBuffer, sizeof(Settings) / 4, &Settings, 0);
	pCommandContext->SetComputeRootDescriptorTable(RootParameters::Accumulation::Input, pPathtracingRenderPass->ResourceViews[Pathtracing::EResourceViews::RenderTargetUnorderedAccess].GetStartDescriptor().GPUHandle);
	pCommandContext->SetComputeRootDescriptorTable(RootParameters::Accumulation::Output, ResourceViews[EResourceViews::RenderTargetUnorderedAccess].GetStartDescriptor().GPUHandle);

	UINT threadGroupCountX = Math::RoundUpAndDivide(pOutput->GetWidth(), 16);
	UINT threadGroupCountY = Math::RoundUpAndDivide(pOutput->GetHeight(), 16);
	pCommandContext->Dispatch(threadGroupCountX, threadGroupCountY, 1);

	pCommandContext->TransitionBarrier(pOutput, Resource::State::NonPixelShaderResource);
}

void Accumulation::Resize(UINT Width, UINT Height, RenderDevice* pRenderDevice)
{
	// Destroy render target
	for (auto& output : Resources)
	{
		pRenderDevice->Destroy(&output);
	}

	// Recreate render target
	Resources[EResources::RenderTarget] = pRenderDevice->CreateTexture(Resource::Type::Texture2D, [&](TextureProxy& proxy)
	{
		proxy.SetFormat(RendererFormats::HDRBufferFormat);
		proxy.SetWidth(Width);
		proxy.SetHeight(Height);
		proxy.BindFlags = Resource::BindFlags::UnorderedAccess;
		proxy.InitialState = Resource::State::UnorderedAccess;
	});

	// Recreate uav for render target
	const auto& uav = ResourceViews[EResourceViews::RenderTargetUnorderedAccess];
	pRenderDevice->CreateUAV(Resources[EResources::RenderTarget], uav[0], {}, {});

	// Recreate SRV for render targets
	const auto& srv = ResourceViews[EResourceViews::RenderTargetShaderResource];
	pRenderDevice->CreateSRV(Resources[EResources::RenderTarget], srv[0]);
}