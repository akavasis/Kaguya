#include "pch.h"
#include "Accumulation.h"

#include "Pathtracing.h"

Accumulation::Accumulation(UINT Width, UINT Height)
	: RenderPass(RenderPassType::Graphics, { Width, Height, RendererFormats::HDRBufferFormat })
{

}

Accumulation::~Accumulation()
{

}

bool Accumulation::Initialize(RenderDevice* pRenderDevice)
{
	Resources.resize(EResources::NumResources);
	ResourceViews.resize(EResourceViews::NumResourceViews);

	Resources[EResources::RenderTarget] = pRenderDevice->CreateTexture(Resource::Type::Texture2D, [&](TextureProxy& proxy)
	{
		proxy.SetFormat(Properties.Format);
		proxy.SetWidth(Properties.Width);
		proxy.SetHeight(Properties.Height);
		proxy.BindFlags = Resource::BindFlags::UnorderedAccess;
		proxy.InitialState = Resource::State::UnorderedAccess;
	});

	ResourceViews[EResourceViews::RenderTargetUAV] = pRenderDevice->DescriptorAllocator.AllocateUADescriptors(1);
	ResourceViews[EResourceViews::RenderTargetSRV] = pRenderDevice->DescriptorAllocator.AllocateSRDescriptors(1);

	pRenderDevice->CreateUAV(Resources[EResources::RenderTarget], ResourceViews[EResourceViews::RenderTargetUAV].GetStartDescriptor());
	pRenderDevice->CreateSRV(Resources[EResources::RenderTarget], ResourceViews[EResourceViews::RenderTargetSRV].GetStartDescriptor());

	return true;
}

void Accumulation::InitializeScene(GpuScene* pGpuScene, RenderDevice* pRenderDevice)
{
	this->pGpuScene = pGpuScene;

	LastAperture = pGpuScene->pScene->Camera.Aperture;
	LastFocalLength = pGpuScene->pScene->Camera.FocalLength;
	LastCameraTransform = pGpuScene->pScene->Camera.Transform;
}

void Accumulation::RenderGui()
{
	
}

void Accumulation::Execute(RenderGraphRegistry& RenderGraphRegistry, CommandContext* pCommandContext)
{
	PIXMarker(pCommandContext->GetD3DCommandList(), L"Accumulation");

	// If the camera has moved
	if (pGpuScene->pScene->Camera.Aperture != LastAperture ||
		pGpuScene->pScene->Camera.FocalLength != LastFocalLength ||
		pGpuScene->pScene->Camera.Transform != LastCameraTransform)
	{
		Settings.AccumulationCount = 0;
		LastAperture = pGpuScene->pScene->Camera.Aperture;
		LastFocalLength = pGpuScene->pScene->Camera.FocalLength;
		LastCameraTransform = pGpuScene->pScene->Camera.Transform;
	}

	auto pPathtracingRenderPass = RenderGraphRegistry.GetRenderPass<Pathtracing>();

	auto pOutput = RenderGraphRegistry.GetTexture(Resources[EResources::RenderTarget]);

	auto pAccumulationPipelineState = RenderGraphRegistry.GetComputePSO(ComputePSOs::Accumulation);
	auto pAccumulationRootSignature = RenderGraphRegistry.GetRootSignature(RootSignatures::Raytracing::Accumulation);

	pCommandContext->TransitionBarrier(pOutput, Resource::State::UnorderedAccess);

	// Bind Pipeline
	pCommandContext->SetPipelineState(pAccumulationPipelineState);
	pCommandContext->SetComputeRootSignature(pAccumulationRootSignature);

	// Bind Resources
	struct  
	{
		unsigned int AccumulationCount;
	} AccumulationSettings;
	AccumulationSettings.AccumulationCount = Settings.AccumulationCount++;
	pCommandContext->SetComputeRoot32BitConstants(0, 1, &AccumulationSettings, 0);
	pCommandContext->SetComputeRootDescriptorTable(1, pPathtracingRenderPass->ResourceViews[Pathtracing::EResourceViews::RenderTargetUAV].GetStartDescriptor().GPUHandle);
	pCommandContext->SetComputeRootDescriptorTable(2, ResourceViews[EResourceViews::RenderTargetUAV].GetStartDescriptor().GPUHandle);

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
	const auto& uav = ResourceViews[EResourceViews::RenderTargetUAV];
	pRenderDevice->CreateUAV(Resources[EResources::RenderTarget], uav[0], {}, {});

	// Recreate SRV for render targets
	const auto& srv = ResourceViews[EResourceViews::RenderTargetSRV];
	pRenderDevice->CreateSRV(Resources[EResources::RenderTarget], srv[0]);
}

void Accumulation::StateRefresh()
{
	Settings.AccumulationCount = 0;
}