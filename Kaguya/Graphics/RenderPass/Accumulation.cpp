#include "pch.h"
#include "Accumulation.h"

#include "Pathtracing.h"
#include "RaytraceGBuffer.h"

Accumulation::Accumulation(UINT Width, UINT Height)
	: RenderPass(RenderPassType::Graphics,
		{ Width, Height, RendererFormats::HDRBufferFormat })
{

}

Accumulation::~Accumulation()
{

}

void Accumulation::ScheduleResource(ResourceScheduler* pResourceScheduler)
{
	pResourceScheduler->AllocateTexture(Resource::Type::Texture2D, [&](TextureProxy& proxy)
	{
		proxy.SetFormat(Properties.Format);
		proxy.SetWidth(Properties.Width);
		proxy.SetHeight(Properties.Height);
		proxy.BindFlags = Resource::BindFlags::UnorderedAccess;
		proxy.InitialState = Resource::State::UnorderedAccess;
	});
}

void Accumulation::InitializeScene(GpuScene* pGpuScene, RenderDevice* pRenderDevice)
{
	this->pGpuScene = pGpuScene;

	LastGBuffer = 0;
	LastAperture = pGpuScene->pScene->Camera.Aperture;
	LastFocalLength = pGpuScene->pScene->Camera.FocalLength;
	LastCameraTransform = pGpuScene->pScene->Camera.Transform;
}

void Accumulation::RenderGui()
{

}

void Accumulation::Execute(ResourceRegistry& ResourceRegistry, CommandContext* pCommandContext)
{
	PIXMarker(pCommandContext->GetD3DCommandList(), L"Accumulation");

	//auto pPathtracingRenderPass = RenderGraphRegistry.GetRenderPass<Pathtracing>();
	auto pRaytraceGBufferRenderPass = ResourceRegistry.GetRenderPass<RaytraceGBuffer>();

	// If the camera has moved
	if (pRaytraceGBufferRenderPass->GetSettings().GBuffer != LastGBuffer ||
		pGpuScene->pScene->Camera.Aperture != LastAperture ||
		pGpuScene->pScene->Camera.FocalLength != LastFocalLength ||
		pGpuScene->pScene->Camera.Transform != LastCameraTransform)
	{
		Settings.AccumulationCount = 0;
		LastGBuffer = pRaytraceGBufferRenderPass->GetSettings().GBuffer;
		LastAperture = pGpuScene->pScene->Camera.Aperture;
		LastFocalLength = pGpuScene->pScene->Camera.FocalLength;
		LastCameraTransform = pGpuScene->pScene->Camera.Transform;
	}



	auto pOutput = ResourceRegistry.GetTexture(Resources[EResources::RenderTarget]);

	pCommandContext->TransitionBarrier(pOutput, Resource::State::UnorderedAccess);

	// Bind Pipeline
	pCommandContext->SetPipelineState(ResourceRegistry.GetComputePSO(ComputePSOs::Accumulation));
	pCommandContext->SetComputeRootSignature(ResourceRegistry.GetRootSignature(RootSignatures::Raytracing::Accumulation));

	// Bind Resources
	// + 1 for the constant buffer offset
	size_t srv = ResourceRegistry.GetShaderResourceDescriptorIndex(pRaytraceGBufferRenderPass->Resources[pRaytraceGBufferRenderPass->GetSettings().GBuffer + 1]);
	size_t uav = ResourceRegistry.GetUnorderedAccessDescriptorIndex(Resources[EResources::RenderTarget]);
	struct
	{
		unsigned int AccumulationCount;
	} AccumulationSettings;
	AccumulationSettings.AccumulationCount = Settings.AccumulationCount++;
	pCommandContext->SetComputeRoot32BitConstants(0, 1, &AccumulationSettings, 0);
	//pCommandContext->SetComputeRootDescriptorTable(1, pPathtracingRenderPass->ResourceViews[Pathtracing::EResourceViews::RenderTargetUAV].GetStartDescriptor().GPUHandle);
	pCommandContext->SetComputeRootDescriptorTable(1, ResourceRegistry.ShaderResourceViews[srv].GPUHandle);
	pCommandContext->SetComputeRootDescriptorTable(2, ResourceRegistry.UnorderedAccessViews[uav].GPUHandle);

	UINT threadGroupCountX = Math::RoundUpAndDivide(pOutput->GetWidth(), 16);
	UINT threadGroupCountY = Math::RoundUpAndDivide(pOutput->GetHeight(), 16);
	pCommandContext->Dispatch(threadGroupCountX, threadGroupCountY, 1);

	pCommandContext->TransitionBarrier(pOutput, Resource::State::NonPixelShaderResource);
}

void Accumulation::StateRefresh()
{
	Settings.AccumulationCount = 0;
}