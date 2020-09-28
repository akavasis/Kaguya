#include "pch.h"
#include "Pathtracing.h"
#include "../Renderer.h"

Pathtracing::Pathtracing(UINT Width, UINT Height)
	: RenderPass(RenderPassType::Graphics,
		{ Width, Height, RendererFormats::HDRBufferFormat })
{

}

Pathtracing::~Pathtracing()
{

}

void Pathtracing::ScheduleResource(ResourceScheduler* pResourceScheduler)
{
	pResourceScheduler->AllocateBuffer([](BufferProxy& proxy)
	{
		proxy.SetSizeInBytes(Math::AlignUp<UINT64>(sizeof(RenderPassConstants), D3D12_CONSTANT_BUFFER_DATA_PLACEMENT_ALIGNMENT));

		proxy.SetStride(Math::AlignUp<UINT64>(sizeof(RenderPassConstants), D3D12_CONSTANT_BUFFER_DATA_PLACEMENT_ALIGNMENT));
		proxy.SetCpuAccess(Buffer::CpuAccess::Write);
	});

	pResourceScheduler->AllocateTexture(Resource::Type::Texture2D, [&](TextureProxy& proxy)
	{
		proxy.SetFormat(Properties.Format);
		proxy.SetWidth(Properties.Width);
		proxy.SetHeight(Properties.Height);
		proxy.BindFlags = Resource::BindFlags::UnorderedAccess;
		proxy.InitialState = Resource::State::UnorderedAccess;
	});
}

void Pathtracing::InitializeScene(GpuScene* pGpuScene, RenderDevice* pRenderDevice)
{
	this->pGpuScene = pGpuScene;

	RaytracingPipelineState* pRaytracingPipelineState = pRenderDevice->GetRaytracingPSO(RaytracingPSOs::Pathtracing);

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

void Pathtracing::RenderGui()
{
	if (ImGui::TreeNode("Path Tracing"))
	{
		if (ImGui::Button("Restore Defaults"))
		{
			Settings = SSettings();
			Refresh = true;
		}
		if (ImGui::SliderInt("Max Depth", &Settings.MaxDepth, 1, 7))
		{
			Refresh = true;
		}
		ImGui::TreePop();
	}
}

void Pathtracing::Execute(ResourceRegistry& ResourceRegistry, CommandContext* pCommandContext)
{
	PIXMarker(pCommandContext->GetD3DCommandList(), L"Pathtracing");

	auto pConstantBuffer = ResourceRegistry.GetBuffer(Resources[EResources::ConstantBuffer]);
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

	renderPassCPU.MaxDepth = Settings.MaxDepth;
	renderPassCPU.FocalLength = pGpuScene->pScene->Camera.FocalLength;
	renderPassCPU.LensRadius = pGpuScene->pScene->Camera.Aperture;

	pConstantBuffer->Update<RenderPassConstants>(0, renderPassCPU);

	auto pOutput = ResourceRegistry.GetTexture(Resources[EResources::RenderTarget]);
	auto pRayGenerationShaderTable = ResourceRegistry.GetBuffer(m_RayGenerationShaderTable);
	auto pMissShaderTable = ResourceRegistry.GetBuffer(m_MissShaderTable);
	auto pHitGroupShaderTable = ResourceRegistry.GetBuffer(m_HitGroupShaderTable);

	size_t uav = ResourceRegistry.GetUnorderedAccessDescriptorIndex(Resources[EResources::RenderTarget]);

	pCommandContext->SetRaytracingPipelineState(ResourceRegistry.GetRaytracingPSO(RaytracingPSOs::Pathtracing));
	pCommandContext->SetComputeRootSignature(ResourceRegistry.GetRootSignature(RootSignatures::Raytracing::Pathtracing::Global));

	pCommandContext->SetComputeRootDescriptorTable(RootParameters::Raytracing::GeometryTable, pGpuScene->ShaderResourceViews.GetStartDescriptor().GPUHandle);
	pCommandContext->SetComputeRootDescriptorTable(RootParameters::Raytracing::RenderTarget, ResourceRegistry.UnorderedAccessViews[uav].GPUHandle);
	pCommandContext->SetComputeRootConstantBufferView(RootParameters::StandardShaderLayout::RenderPassDataCB + RootParameters::Raytracing::NumRootParameters,
		pConstantBuffer->GetGpuVirtualAddressAt(0));
	pCommandContext->SetComputeRootDescriptorTable(RootParameters::StandardShaderLayout::DescriptorTables + RootParameters::Raytracing::NumRootParameters,
		ResourceRegistry.GetUniversalGpuDescriptorHeapSRVDescriptorHandleFromStart());

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

	pCommandContext->DispatchRays(&desc);

	pCommandContext->UAVBarrier(pOutput);
}

void Pathtracing::StateRefresh()
{
}