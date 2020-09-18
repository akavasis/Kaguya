#include "pch.h"
#include "Raytracing.h"

Raytracing::Raytracing(UINT Width, UINT Height, GpuBufferAllocator* pGpuBufferAllocator, GpuTextureAllocator* pGpuTextureAllocator, Buffer* pRenderPassConstantBuffer)
	: IRenderPass(RenderPassType::Graphics, { Width, Height, RendererFormats::HDRBufferFormat }),
	pGpuBufferAllocator(pGpuBufferAllocator), pGpuTextureAllocator(pGpuTextureAllocator), pRenderPassConstantBuffer(pRenderPassConstantBuffer)
{

}

Raytracing::~Raytracing()
{

}

void Raytracing::Setup(RenderDevice* pRenderDevice)
{
	auto raytracingOutput = pRenderDevice->CreateTexture(Resource::Type::Texture2D, [&](TextureProxy& proxy)
	{
		proxy.SetFormat(Properties.Format);
		proxy.SetWidth(Properties.Width);
		proxy.SetHeight(Properties.Height);
		proxy.BindFlags = Resource::BindFlags::UnorderedAccess;
		proxy.InitialState = Resource::State::UnorderedAccess;
	});

	Outputs.push_back(raytracingOutput);

	auto srv = pRenderDevice->DescriptorAllocator.AllocateSRDescriptors(5);
	pRenderDevice->CreateSRV(pGpuBufferAllocator->GetRTTLASResourceHandle(), srv[0]);
	pRenderDevice->CreateSRV(pGpuBufferAllocator->GetVertexBufferHandle(), srv[1]);
	pRenderDevice->CreateSRV(pGpuBufferAllocator->GetIndexBufferHandle(), srv[2]);
	pRenderDevice->CreateSRV(pGpuBufferAllocator->GetGeometryInfoBufferHandle(), srv[3]);
	pRenderDevice->CreateSRV(pGpuTextureAllocator->GetMaterialBufferHandle(), srv[4]);

	auto uav = pRenderDevice->DescriptorAllocator.AllocateUADescriptors(1);
	pRenderDevice->CreateUAV(raytracingOutput, uav[0], {}, {});

	ResourceViews.push_back(std::move(srv));
	ResourceViews.push_back(std::move(uav));
}

void Raytracing::Update()
{

}

void Raytracing::RenderGui()
{

}

void Raytracing::Execute(const Scene& Scene, RenderGraphRegistry& RenderGraphRegistry, CommandContext* pCommandContext)
{
	PIXMarker(pCommandContext->GetD3DCommandList(), L"Raytracing");

	auto pOutput = RenderGraphRegistry.GetTexture(Outputs[EOutputs::RenderTarget]);
	auto pRayGenerationShaderTable = RenderGraphRegistry.GetBuffer(pGpuBufferAllocator->GetRayGenerationShaderTableHandle());
	auto pMissShaderTable = RenderGraphRegistry.GetBuffer(pGpuBufferAllocator->GetMissShaderTableHandle());
	auto pHitGroupShaderTable = RenderGraphRegistry.GetBuffer(pGpuBufferAllocator->GetHitGroupShaderTableHandle());

	auto pRaytracingPipelineState = RenderGraphRegistry.GetRaytracingPSO(RaytracingPSOs::Raytracing);

	pGpuBufferAllocator->Bind(pCommandContext);

	pCommandContext->SetComputeRootSignature(RenderGraphRegistry.GetRootSignature(RootSignatures::Raytracing::Global));

	pCommandContext->SetComputeRootDescriptorTable(RootParameters::Raytracing::GeometryTable, ResourceViews[EResourceViews::GeometryTable].GetStartDescriptor().GPUHandle);
	pCommandContext->SetComputeRootDescriptorTable(RootParameters::Raytracing::RenderTarget, ResourceViews[EResourceViews::RenderTarget].GetStartDescriptor().GPUHandle);
	pCommandContext->SetComputeRootConstantBufferView(RootParameters::StandardShaderLayout::RenderPassDataCB + RootParameters::Raytracing::NumRootParameters,
		pRenderPassConstantBuffer->GetGpuVirtualAddressAt(0));
	pCommandContext->SetComputeRootDescriptorTable(RootParameters::StandardShaderLayout::DescriptorTables + RootParameters::Raytracing::NumRootParameters,
		RenderGraphRegistry.GetUniversalGpuDescriptorHeapSRVDescriptorHandleFromStart());

	D3D12_DISPATCH_RAYS_DESC desc = {};

	desc.RayGenerationShaderRecord.StartAddress = pRayGenerationShaderTable->GetGpuVirtualAddress();
	desc.RayGenerationShaderRecord.SizeInBytes = pRayGenerationShaderTable->GetMemoryRequested();

	desc.MissShaderTable.StartAddress = pMissShaderTable->GetGpuVirtualAddress();
	desc.MissShaderTable.SizeInBytes = pMissShaderTable->GetMemoryRequested();
	desc.MissShaderTable.StrideInBytes = pMissShaderTable->GetStride();

	desc.HitGroupTable.StartAddress = pHitGroupShaderTable->GetGpuVirtualAddress();
	desc.HitGroupTable.SizeInBytes = pHitGroupShaderTable->GetMemoryRequested();
	desc.HitGroupTable.StrideInBytes = pHitGroupShaderTable->GetStride();

	desc.Width = pOutput->GetWidth();
	desc.Height = pOutput->GetHeight();
	desc.Depth = 1;

	pCommandContext->SetRaytracingPipelineState(pRaytracingPipelineState);
	pCommandContext->DispatchRays(&desc);

	pCommandContext->UAVBarrier(pOutput);
}

void Raytracing::Resize(UINT Width, UINT Height, RenderDevice* pRenderDevice)
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

	// Recreate resource views
	const auto& uav = ResourceViews[EResourceViews::RenderTarget];
	pRenderDevice->CreateUAV(Outputs[EOutputs::RenderTarget], uav[0], {}, {});
}