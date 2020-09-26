#include "pch.h"
#include "RaytraceGBuffer.h"
#include "../Renderer.h"

RaytraceGBuffer::RaytraceGBuffer(UINT Width, UINT Height)
	: RenderPass(RenderPassType::Graphics,
		{ Width, Height, DXGI_FORMAT_R32G32B32A32_FLOAT },
		EResources::NumResources,
		EResourceViews::NumResourceViews)
{

}

RaytraceGBuffer::~RaytraceGBuffer()
{

}

bool RaytraceGBuffer::Initialize(RenderDevice* pRenderDevice)
{
	Resources[EResources::ConstantBuffer] = pRenderDevice->CreateBuffer([](BufferProxy& proxy)
	{
		proxy.SetSizeInBytes(Math::AlignUp<UINT64>(sizeof(RenderPassConstants), D3D12_CONSTANT_BUFFER_DATA_PLACEMENT_ALIGNMENT));

		proxy.SetStride(Math::AlignUp<UINT64>(sizeof(RenderPassConstants), D3D12_CONSTANT_BUFFER_DATA_PLACEMENT_ALIGNMENT));
		proxy.SetCpuAccess(Buffer::CpuAccess::Write);
	});

	for (size_t i = 1; i < EResources::NumResources; ++i)
	{
		Resources[i] = pRenderDevice->CreateTexture(Resource::Type::Texture2D, [&](TextureProxy& proxy)
		{
			proxy.SetFormat(Properties.Format);
			proxy.SetWidth(Properties.Width);
			proxy.SetHeight(Properties.Height);
			proxy.BindFlags = Resource::BindFlags::UnorderedAccess;
			proxy.InitialState = Resource::State::UnorderedAccess;
		});
	}

	ResourceViews[EResourceViews::GeometryTables] = pRenderDevice->DescriptorAllocator.AllocateSRDescriptors(5);
	ResourceViews[EResourceViews::RenderTargetUAVs] = pRenderDevice->DescriptorAllocator.AllocateUADescriptors(EResources::NumResources- 1);

	for (size_t i = 1; i < EResources::NumResources; ++i)
	{
		pRenderDevice->CreateUAV(Resources[i], ResourceViews[EResourceViews::RenderTargetUAVs][i - 1]);
	}

	return true;
}

void RaytraceGBuffer::InitializeScene(GpuScene* pGpuScene, RenderDevice* pRenderDevice)
{
	this->pGpuScene = pGpuScene;

	pRenderDevice->CreateSRV(pGpuScene->GetRTTLASResourceHandle(), ResourceViews[EResourceViews::GeometryTables][0]);
	pRenderDevice->CreateSRV(pGpuScene->GetVertexBufferHandle(), ResourceViews[EResourceViews::GeometryTables][1]);
	pRenderDevice->CreateSRV(pGpuScene->GetIndexBufferHandle(), ResourceViews[EResourceViews::GeometryTables][2]);
	pRenderDevice->CreateSRV(pGpuScene->GetGeometryInfoTableHandle(), ResourceViews[EResourceViews::GeometryTables][3]);
	pRenderDevice->CreateSRV(pGpuScene->GetMaterialTableHandle(), ResourceViews[EResourceViews::GeometryTables][4]);

	RaytracingPipelineState* pRaytracingPipelineState = pRenderDevice->GetRaytracingPSO(RaytracingPSOs::RaytraceGBuffer);

	// Ray Generation Shader Table
	{
		ShaderTable<void> shaderTable;
		shaderTable.AddShaderRecord(pRaytracingPipelineState->GetShaderIdentifier(L"RayGeneration"));

		UINT64 shaderTableSizeInBytes; UINT stride;
		shaderTable.ComputeMemoryRequirements(&shaderTableSizeInBytes);
		stride = shaderTable.GetShaderRecordStride();

		m_RayGenerationShaderTable = pRenderDevice->CreateBuffer([shaderTableSizeInBytes, stride](BufferProxy& proxy)
		{
			proxy.SetSizeInBytes(shaderTableSizeInBytes);
			proxy.SetStride(stride);
			proxy.SetCpuAccess(Buffer::CpuAccess::Write);
		});

		Buffer* pShaderTableBuffer = pRenderDevice->GetBuffer(m_RayGenerationShaderTable);
		shaderTable.Generate(pShaderTableBuffer);
	}

	// Miss Shader Table
	{
		ShaderTable<void> shaderTable;
		shaderTable.AddShaderRecord(pRaytracingPipelineState->GetShaderIdentifier(L"Miss"));

		UINT64 shaderTableSizeInBytes; UINT stride;
		shaderTable.ComputeMemoryRequirements(&shaderTableSizeInBytes);
		stride = shaderTable.GetShaderRecordStride();

		m_MissShaderTable = pRenderDevice->CreateBuffer([shaderTableSizeInBytes, stride](BufferProxy& proxy)
		{
			proxy.SetSizeInBytes(shaderTableSizeInBytes);
			proxy.SetStride(stride);
			proxy.SetCpuAccess(Buffer::CpuAccess::Write);
		});

		Buffer* pShaderTableBuffer = pRenderDevice->GetBuffer(m_MissShaderTable);
		shaderTable.Generate(pShaderTableBuffer);
	}

	// Hit Group Shader Table
	{
		ShaderTable<void> shaderTable;

		ShaderIdentifier hitGroupSID = pRaytracingPipelineState->GetShaderIdentifier(L"Default");

		for (const auto& modelInstance : pGpuScene->pScene->ModelInstances)
		{
			for (const auto& meshInstance : modelInstance.MeshInstances)
			{
				shaderTable.AddShaderRecord(hitGroupSID);
			}
		}

		UINT64 shaderTableSizeInBytes; UINT stride;
		shaderTable.ComputeMemoryRequirements(&shaderTableSizeInBytes);
		stride = shaderTable.GetShaderRecordStride();

		m_HitGroupShaderTable = pRenderDevice->CreateBuffer([shaderTableSizeInBytes, stride](BufferProxy& proxy)
		{
			proxy.SetSizeInBytes(shaderTableSizeInBytes);
			proxy.SetStride(stride);
			proxy.SetCpuAccess(Buffer::CpuAccess::Write);
		});

		Buffer* pShaderTableBuffer = pRenderDevice->GetBuffer(m_HitGroupShaderTable);
		shaderTable.Generate(pShaderTableBuffer);
	}
}

void RaytraceGBuffer::RenderGui()
{

}

void RaytraceGBuffer::Execute(RenderGraphRegistry& RenderGraphRegistry, CommandContext* pCommandContext)
{
	PIXMarker(pCommandContext->GetD3DCommandList(), L"Raytrace GBuffer");

	auto pConstantBuffer = RenderGraphRegistry.GetBuffer(Resources[EResources::ConstantBuffer]);
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

	renderPassCPU.MaxDepth = 1;
	renderPassCPU.FocalLength = pGpuScene->pScene->Camera.FocalLength;
	renderPassCPU.LensRadius = pGpuScene->pScene->Camera.Aperture;

	pConstantBuffer->Update<RenderPassConstants>(0, renderPassCPU);

	auto pWorldPosition = RenderGraphRegistry.GetTexture(Resources[EResources::WorldPosition]);
	auto pWorldNormal = RenderGraphRegistry.GetTexture(Resources[EResources::WorldNormal]);
	auto pMaterialAlbedo = RenderGraphRegistry.GetTexture(Resources[EResources::MaterialAlbedo]);
	auto pMaterialEmissive = RenderGraphRegistry.GetTexture(Resources[EResources::MaterialEmissive]);
	auto pMaterialSpecular = RenderGraphRegistry.GetTexture(Resources[EResources::MaterialSpecular]);
	auto pMaterialRefraction = RenderGraphRegistry.GetTexture(Resources[EResources::MaterialRefraction]);
	auto pMaterialExtra = RenderGraphRegistry.GetTexture(Resources[EResources::MaterialExtra]);

	auto pRayGenerationShaderTable = RenderGraphRegistry.GetBuffer(m_RayGenerationShaderTable);
	auto pMissShaderTable = RenderGraphRegistry.GetBuffer(m_MissShaderTable);
	auto pHitGroupShaderTable = RenderGraphRegistry.GetBuffer(m_HitGroupShaderTable);

	auto pRaytracingPipelineState = RenderGraphRegistry.GetRaytracingPSO(RaytracingPSOs::RaytraceGBuffer);

	pCommandContext->SetComputeRootSignature(RenderGraphRegistry.GetRootSignature(RootSignatures::Raytracing::RaytraceGBuffer::Global));

	pCommandContext->SetComputeRootDescriptorTable(RootParameters::Raytracing::GeometryTable, ResourceViews[EResourceViews::GeometryTables].GetStartDescriptor().GPUHandle);
	pCommandContext->SetComputeRootDescriptorTable(RootParameters::Raytracing::RenderTarget, ResourceViews[EResourceViews::RenderTargetUAVs].GetStartDescriptor().GPUHandle);
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

	pCommandContext->UAVBarrier(pWorldPosition);
	pCommandContext->UAVBarrier(pWorldNormal);
	pCommandContext->UAVBarrier(pMaterialAlbedo);
	pCommandContext->UAVBarrier(pMaterialEmissive);
	pCommandContext->UAVBarrier(pMaterialSpecular);
	pCommandContext->UAVBarrier(pMaterialRefraction);
	pCommandContext->UAVBarrier(pMaterialExtra);
}

void RaytraceGBuffer::Resize(UINT Width, UINT Height, RenderDevice* pRenderDevice)
{

}

void RaytraceGBuffer::StateRefresh()
{

}
