#include "pch.h"
#include "Pathtracing.h"
#include "../Renderer.h"

Pathtracing::Pathtracing(UINT Width, UINT Height)
	: IRenderPass(RenderPassType::Graphics, { Width, Height, RendererFormats::HDRBufferFormat })
{

}

Pathtracing::~Pathtracing()
{

}

bool Pathtracing::Initialize(GpuScene* pGpuScene, RenderDevice* pRenderDevice)
{
	auto constantBufferHandle = pRenderDevice->CreateBuffer([](BufferProxy& proxy)
	{
		proxy.SetSizeInBytes(Math::AlignUp<UINT64>(sizeof(RenderPassConstants), D3D12_CONSTANT_BUFFER_DATA_PLACEMENT_ALIGNMENT));

		proxy.SetStride(Math::AlignUp<UINT64>(sizeof(RenderPassConstants), D3D12_CONSTANT_BUFFER_DATA_PLACEMENT_ALIGNMENT));
		proxy.SetCpuAccess(Buffer::CpuAccess::Write);
	});

	auto renderTargetHandle = pRenderDevice->CreateTexture(Resource::Type::Texture2D, [&](TextureProxy& proxy)
	{
		proxy.SetFormat(Properties.Format);
		proxy.SetWidth(Properties.Width);
		proxy.SetHeight(Properties.Height);
		proxy.BindFlags = Resource::BindFlags::UnorderedAccess;
		proxy.InitialState = Resource::State::UnorderedAccess;
	});

	Resources.push_back(constantBufferHandle);
	Resources.push_back(renderTargetHandle);

	auto geometryTablesSRV = pRenderDevice->DescriptorAllocator.AllocateSRDescriptors(5);
	pRenderDevice->CreateSRV(pGpuScene->GetRTTLASResourceHandle(), geometryTablesSRV[0]);
	pRenderDevice->CreateSRV(pGpuScene->GetVertexBufferHandle(), geometryTablesSRV[1]);
	pRenderDevice->CreateSRV(pGpuScene->GetIndexBufferHandle(), geometryTablesSRV[2]);
	pRenderDevice->CreateSRV(pGpuScene->GetGeometryInfoTableHandle(), geometryTablesSRV[3]);
	pRenderDevice->CreateSRV(pGpuScene->GetMaterialTableHandle(), geometryTablesSRV[4]);

	auto renderTargetUAV = pRenderDevice->DescriptorAllocator.AllocateUADescriptors(1);
	pRenderDevice->CreateUAV(renderTargetHandle, renderTargetUAV[0], {}, {});

	ResourceViews.push_back(std::move(geometryTablesSRV));
	ResourceViews.push_back(std::move(renderTargetUAV));

	m_RayGenerationShaderTable = pGpuScene->GetRayGenerationShaderTableHandle();
	m_MissShaderTable = pGpuScene->GetMissShaderTableHandle();
	m_HitGroupShaderTable = pGpuScene->GetHitGroupShaderTableHandle();

	return true;
}

void Pathtracing::Update(GpuScene* pGpuScene, RenderDevice* pRenderDevice)
{
	auto pConstantBuffer = pRenderDevice->GetBuffer(Resources[EResources::ConstantBuffer]);
	pConstantBuffer->Map();

	// Update render pass cbuffer
	RenderPassConstants renderPassCPU;
	XMStoreFloat3(&renderPassCPU.CameraU, pGpuScene->pScene->Camera.GetUVector());
	XMStoreFloat3(&renderPassCPU.CameraV, pGpuScene->pScene->Camera.GetVVector());
	XMStoreFloat3(&renderPassCPU.CameraW, pGpuScene->pScene->Camera.GetWVector());
	XMStoreFloat4x4(&renderPassCPU.View, XMMatrixTranspose(pGpuScene->pScene->Camera.ViewMatrix()));
	XMStoreFloat4x4(&renderPassCPU.Projection, XMMatrixTranspose(pGpuScene->pScene->Camera.ProjectionMatrix()));
	XMStoreFloat4x4(&renderPassCPU.InvView, XMMatrixTranspose(pGpuScene->pScene->Camera.InverseViewMatrix()));
	XMStoreFloat4x4(&renderPassCPU.InvProjection, XMMatrixTranspose(pGpuScene->pScene->Camera.InverseProjectionMatrix()));
	XMStoreFloat4x4(&renderPassCPU.ViewProjection, XMMatrixTranspose(pGpuScene->pScene->Camera.ViewProjectionMatrix()));
	renderPassCPU.EyePosition = pGpuScene->pScene->Camera.Transform.Position;
	renderPassCPU.TotalFrameCount = static_cast<unsigned int>(Renderer::Statistics::TotalFrameCount);

	renderPassCPU.Sun = pGpuScene->pScene->Sun;
	renderPassCPU.BRDFLUTMapIndex = GpuTextureAllocator::RendererReseveredTextures::BRDFLUT;
	renderPassCPU.RadianceCubemapIndex = GpuTextureAllocator::RendererReseveredTextures::SkyboxCubemap;
	renderPassCPU.IrradianceCubemapIndex = GpuTextureAllocator::RendererReseveredTextures::SkyboxIrradianceCubemap;
	renderPassCPU.PrefilteredRadianceCubemapIndex = GpuTextureAllocator::RendererReseveredTextures::SkyboxPrefilteredCubemap;

	renderPassCPU.MaxDepth = 4;
	renderPassCPU.FocalLength = pGpuScene->pScene->Camera.FocalLength;
	renderPassCPU.LensRadius = pGpuScene->pScene->Camera.Aperture;

	pConstantBuffer->Update<RenderPassConstants>(0, renderPassCPU);
}

void Pathtracing::RenderGui()
{

}

void Pathtracing::Execute(RenderGraphRegistry& RenderGraphRegistry, CommandContext* pCommandContext)
{
	PIXMarker(pCommandContext->GetD3DCommandList(), L"Pathtracing");

	auto pConstantBuffer = RenderGraphRegistry.GetBuffer(Resources[EResources::ConstantBuffer]);
	auto pOutput = RenderGraphRegistry.GetTexture(Resources[EResources::RenderTarget]);
	auto pRayGenerationShaderTable = RenderGraphRegistry.GetBuffer(m_RayGenerationShaderTable);
	auto pMissShaderTable = RenderGraphRegistry.GetBuffer(m_MissShaderTable);
	auto pHitGroupShaderTable = RenderGraphRegistry.GetBuffer(m_HitGroupShaderTable);

	auto pRaytracingPipelineState = RenderGraphRegistry.GetRaytracingPSO(RaytracingPSOs::Pathtracing);

	pCommandContext->SetComputeRootSignature(RenderGraphRegistry.GetRootSignature(RootSignatures::Raytracing::Pathtracing::Global));

	pCommandContext->SetComputeRootDescriptorTable(RootParameters::Raytracing::GeometryTable, ResourceViews[EResourceViews::GeometryTables].GetStartDescriptor().GPUHandle);
	pCommandContext->SetComputeRootDescriptorTable(RootParameters::Raytracing::RenderTarget, ResourceViews[EResourceViews::RenderTargetUnorderedAccess].GetStartDescriptor().GPUHandle);
	pCommandContext->SetComputeRootConstantBufferView(RootParameters::StandardShaderLayout::RenderPassDataCB + RootParameters::Raytracing::NumRootParameters,
		pConstantBuffer->GetGpuVirtualAddressAt(0));
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

	desc.Width = Properties.Width;
	desc.Height = Properties.Height;
	desc.Depth = 1;

	pCommandContext->SetRaytracingPipelineState(pRaytracingPipelineState);
	pCommandContext->DispatchRays(&desc);

	pCommandContext->UAVBarrier(pOutput);
}

void Pathtracing::Resize(UINT Width, UINT Height, RenderDevice* pRenderDevice)
{
	IRenderPass::Resize(Width, Height, pRenderDevice);

	// Destroy render target
	pRenderDevice->Destroy(&Resources[EResources::RenderTarget]);

	// Recreate render target
	Resources[EResources::RenderTarget] = pRenderDevice->CreateTexture(Resource::Type::Texture2D, [&](TextureProxy& proxy)
	{
		proxy.SetFormat(RendererFormats::HDRBufferFormat);
		proxy.SetWidth(Width);
		proxy.SetHeight(Height);
		proxy.BindFlags = Resource::BindFlags::UnorderedAccess;
		proxy.InitialState = Resource::State::UnorderedAccess;
	});

	// Recreate resource views
	const auto& uav = ResourceViews[EResourceViews::RenderTargetUnorderedAccess];
	pRenderDevice->CreateUAV(Resources[EResources::RenderTarget], uav[0], {}, {});
}