#include "pch.h"
#include "PathIntegrator.h"

#include "RendererRegistry.h"

#include <ResourceUploadBatch.h>

using namespace DirectX;

namespace
{
	// Symbols
	constexpr LPCWSTR RayGeneration = L"RayGeneration";
	constexpr LPCWSTR Miss = L"Miss";
	constexpr LPCWSTR ShadowMiss = L"ShadowMiss";
	constexpr LPCWSTR ClosestHit = L"ClosestHit";

	// HitGroup Exports
	constexpr LPCWSTR HitGroupExport = L"Default";

	ShaderIdentifier RayGenerationSID;
	ShaderIdentifier MissSID;
	ShaderIdentifier ShadowMissSID;
	ShaderIdentifier DefaultSID;
}

void PathIntegrator::Settings::RenderGui()
{
	if (ImGui::TreeNode("Path Integrator"))
	{
		if (ImGui::Button("Restore Defaults"))
		{
			Settings::RestoreDefaults();
		}

		bool Dirty = false;
		Dirty |= ImGui::SliderScalar("Num Samples Per Pixel", ImGuiDataType_U32, &NumSamplesPerPixel, &MinimumSamples, &MaximumSamples);
		Dirty |= ImGui::SliderScalar("Max Depth", ImGuiDataType_U32, &MaxDepth, &MinimumDepth, &MaximumDepth);
		ImGui::Text("Num Samples Accumulated: %u", NumAccumulatedSamples);

		if (Dirty)
		{
			if (pPathIntegrator)
			{
				pPathIntegrator->Reset();
			}
		}

		ImGui::TreePop();
	}
}

void PathIntegrator::Create()
{
	Settings::RestoreDefaults();
	Settings::pPathIntegrator = this;

	auto& RenderDevice = RenderDevice::Instance();

	UAV = RenderDevice.AllocateUnorderedAccessView();
	SRV = RenderDevice.AllocateShaderResourceView();

	GlobalRS = RenderDevice.CreateRootSignature([](RootSignatureBuilder& Builder)
	{
		Builder.AddRootCBVParameter(RootCBV(0, 0)); // g_SystemConstants		b0 | space0
		Builder.AddRootCBVParameter(RootCBV(1, 0)); // g_RenderPassData			b1 | space0

		Builder.AddRootSRVParameter(RootSRV(0, 0));	// g_Scene					t0 | space0
		Builder.AddRootSRVParameter(RootSRV(1, 0));	// g_Materials				t1 | space0
		Builder.AddRootSRVParameter(RootSRV(2, 0));	// g_Lights					t2 | space0

		Builder.AddStaticSampler(0, D3D12_FILTER_MIN_MAG_MIP_POINT, D3D12_TEXTURE_ADDRESS_MODE_WRAP, 16);	// g_SamplerPointWrap			s0 | space0;
		Builder.AddStaticSampler(1, D3D12_FILTER_MIN_MAG_MIP_POINT, D3D12_TEXTURE_ADDRESS_MODE_CLAMP, 16);	// g_SamplerPointClamp			s1 | space0;
		Builder.AddStaticSampler(2, D3D12_FILTER_MIN_MAG_MIP_LINEAR, D3D12_TEXTURE_ADDRESS_MODE_WRAP, 16);	// g_SamplerLinearWrap			s2 | space0;
		Builder.AddStaticSampler(3, D3D12_FILTER_MIN_MAG_MIP_LINEAR, D3D12_TEXTURE_ADDRESS_MODE_CLAMP, 16); // g_SamplerLinearClamp			s3 | space0;
		Builder.AddStaticSampler(4, D3D12_FILTER_ANISOTROPIC, D3D12_TEXTURE_ADDRESS_MODE_WRAP, 16);			// g_SamplerAnisotropicWrap		s4 | space0;
		Builder.AddStaticSampler(5, D3D12_FILTER_ANISOTROPIC, D3D12_TEXTURE_ADDRESS_MODE_CLAMP, 16);		// g_SamplerAnisotropicClamp	s5 | space0;
	});

	LocalHitGroupRS = RenderDevice::Instance().CreateRootSignature([](RootSignatureBuilder& Builder)
	{
		Builder.AddRootConstantsParameter<void>(RootConstants<void>(0, 1, 1)); // RootConstants b0 | space1

		Builder.AddRootSRVParameter(RootSRV(0, 1));	// VertexBuffer				t0 | space1
		Builder.AddRootSRVParameter(RootSRV(1, 1));	// IndexBuffer				t1 | space1

		Builder.SetAsLocalRootSignature();
	}, false);

	RTPSO = RenderDevice.CreateRaytracingPipelineState([&](RaytracingPipelineStateBuilder& Builder)
	{
		const Library* pRaytraceLibrary = &Libraries::PathTrace;

		Builder.AddLibrary(pRaytraceLibrary,
			{
				RayGeneration,
				Miss,
				ShadowMiss,
				ClosestHit
			});

		Builder.AddHitGroup(HitGroupExport, nullptr, ClosestHit, nullptr);

		Builder.AddRootSignatureAssociation(&LocalHitGroupRS,
			{
				HitGroupExport
			});

		Builder.SetGlobalRootSignature(&GlobalRS);

		Builder.SetRaytracingShaderConfig(12 * sizeof(float) + 2 * sizeof(unsigned int), SizeOfBuiltInTriangleIntersectionAttributes);

		// +1 for Primary, +1 for Shadow
		Builder.SetRaytracingPipelineConfig(2);
	});

	RayGenerationSID = RTPSO.GetShaderIdentifier(L"RayGeneration");
	MissSID = RTPSO.GetShaderIdentifier(L"Miss");
	ShadowMissSID = RTPSO.GetShaderIdentifier(L"ShadowMiss");
	DefaultSID = RTPSO.GetShaderIdentifier(L"Default");

	ResourceUploadBatch uploader(RenderDevice.Device);

	uploader.Begin(D3D12_COMMAND_LIST_TYPE_COPY);

	D3D12MA::ALLOCATION_DESC allocationDesc = {};
	allocationDesc.Flags = D3D12MA::ALLOCATION_FLAG_COMMITTED;
	allocationDesc.HeapType = D3D12_HEAP_TYPE_DEFAULT;

	// Ray Generation Shader Table
	{
		RayGenerationShaderTable.AddShaderRecord(RayGenerationSID);

		UINT64 shaderTableSizeInBytes = RayGenerationShaderTable.GetSizeInBytes();

		SharedGraphicsResource rayGenSBTUpload = RenderDevice.Device.GraphicsMemory()->Allocate(shaderTableSizeInBytes);
		m_RayGenerationShaderTable = RenderDevice.CreateBuffer(&allocationDesc, shaderTableSizeInBytes);

		RayGenerationShaderTable.AssociateResource(m_RayGenerationShaderTable->pResource.Get());
		RayGenerationShaderTable.Generate(static_cast<BYTE*>(rayGenSBTUpload.Memory()));

		uploader.Upload(RayGenerationShaderTable.Resource(), rayGenSBTUpload);
	}

	// Miss Shader Table
	{
		MissShaderTable.AddShaderRecord(MissSID);
		MissShaderTable.AddShaderRecord(ShadowMissSID);

		UINT64 shaderTableSizeInBytes = MissShaderTable.GetSizeInBytes();

		SharedGraphicsResource missSBTUpload = RenderDevice.Device.GraphicsMemory()->Allocate(shaderTableSizeInBytes);
		m_MissShaderTable = RenderDevice.CreateBuffer(&allocationDesc, shaderTableSizeInBytes);

		MissShaderTable.AssociateResource(m_MissShaderTable->pResource.Get());
		MissShaderTable.Generate(static_cast<BYTE*>(missSBTUpload.Memory()));

		uploader.Upload(m_MissShaderTable->pResource.Get(), missSBTUpload);
	}

	auto finish = uploader.End(RenderDevice.CopyQueue);
	finish.wait();

	m_HitGroupShaderTable = RenderDevice.CreateBuffer(&allocationDesc, HitGroupShaderTable.GetStrideInBytes() * Scene::MAX_INSTANCE_SUPPORTED);

	HitGroupShaderTable.Reserve(Scene::MAX_INSTANCE_SUPPORTED);
	HitGroupShaderTable.AssociateResource(m_HitGroupShaderTable->pResource.Get());
}

void PathIntegrator::SetResolution(UINT Width, UINT Height)
{
	auto& RenderDevice = RenderDevice::Instance();

	if ((this->Width == Width && this->Height == Height))
	{
		return;
	}

	this->Width = Width;
	this->Height = Height;

	D3D12MA::ALLOCATION_DESC allocationDesc = {};
	allocationDesc.Flags = D3D12MA::ALLOCATION_FLAG_COMMITTED;
	allocationDesc.HeapType = D3D12_HEAP_TYPE_DEFAULT;

	auto resourceDesc = CD3DX12_RESOURCE_DESC::Tex2D(DXGI_FORMAT_R32G32B32A32_FLOAT, Width, Height);
	resourceDesc.MipLevels = 1;
	resourceDesc.Flags = D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS;

	m_RenderTarget = RenderDevice.CreateResource(&allocationDesc,
		&resourceDesc, D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE, nullptr);

	RenderDevice.CreateUnorderedAccessView(m_RenderTarget->pResource.Get(), UAV);
	RenderDevice.CreateShaderResourceView(m_RenderTarget->pResource.Get(), SRV);

	Reset();
}

void PathIntegrator::Reset()
{
	Settings::NumAccumulatedSamples = 0;
}

void PathIntegrator::UpdateShaderTable(const RaytracingAccelerationStructure& RaytracingAccelerationStructure, CommandList& CommandList)
{
	auto& RenderDevice = RenderDevice::Instance();

	HitGroupShaderTable.Clear();
	for (auto [i, meshRenderer] : enumerate(RaytracingAccelerationStructure.MeshRenderers))
	{
		auto meshFilter = meshRenderer->pMeshFilter;

		const auto& vertexBuffer = meshFilter->Mesh->VertexResource->pResource;
		const auto& indexBuffer = meshFilter->Mesh->IndexResource->pResource;

		ShaderTable<RootArgument>::Record shaderRecord = {};
		shaderRecord.ShaderIdentifier = DefaultSID;
		shaderRecord.RootArguments = 
		{
			.MaterialIndex = (UINT)i,
			.Padding = 0,
			.VertexBuffer = vertexBuffer->GetGPUVirtualAddress(),
			.IndexBuffer = indexBuffer->GetGPUVirtualAddress()
		};

		HitGroupShaderTable.AddShaderRecord(shaderRecord);
	}

	UINT64 shaderTableSizeInBytes = HitGroupShaderTable.GetSizeInBytes();

	GraphicsResource hitGroupUpload = RenderDevice.Device.GraphicsMemory()->Allocate(shaderTableSizeInBytes);

	HitGroupShaderTable.Generate(static_cast<BYTE*>(hitGroupUpload.Memory()));

	CommandList.TransitionBarrier(m_HitGroupShaderTable->pResource.Get(), D3D12_RESOURCE_STATE_COPY_DEST);
	CommandList->CopyBufferRegion(m_HitGroupShaderTable->pResource.Get(), 0, hitGroupUpload.Resource(), hitGroupUpload.ResourceOffset(), hitGroupUpload.Size());
	CommandList.TransitionBarrier(m_HitGroupShaderTable->pResource.Get(), D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE);
}

void PathIntegrator::Render(D3D12_GPU_VIRTUAL_ADDRESS SystemConstants,
	const RaytracingAccelerationStructure& RaytracingAccelerationStructure,
	D3D12_GPU_VIRTUAL_ADDRESS Materials,
	D3D12_GPU_VIRTUAL_ADDRESS Lights,
	CommandList& CommandList)
{
	auto& RenderDevice = RenderDevice::Instance();

	struct RenderPassData
	{
		uint NumSamplesPerPixel;
		uint MaxDepth;
		uint NumAccumulatedSamples;

		uint RenderTarget;
	} g_RenderPassData = {};

	g_RenderPassData.NumSamplesPerPixel = Settings::NumSamplesPerPixel;
	g_RenderPassData.MaxDepth = Settings::MaxDepth;
	g_RenderPassData.NumAccumulatedSamples = Settings::NumAccumulatedSamples++;

	g_RenderPassData.RenderTarget = UAV.Index;

	GraphicsResource constantBuffer = RenderDevice.Device.GraphicsMemory()->AllocateConstant(g_RenderPassData);

	CommandList->SetPipelineState1(RTPSO);
	CommandList->SetComputeRootSignature(GlobalRS);
	CommandList->SetComputeRootConstantBufferView(0, SystemConstants);
	CommandList->SetComputeRootConstantBufferView(1, constantBuffer.GpuAddress());
	CommandList->SetComputeRootShaderResourceView(2, RaytracingAccelerationStructure);
	CommandList->SetComputeRootShaderResourceView(3, Materials);
	CommandList->SetComputeRootShaderResourceView(4, Lights);

	RenderDevice.BindDescriptorTable<PipelineState::Type::Compute>(GlobalRS, CommandList);

	D3D12_DISPATCH_RAYS_DESC desc =
	{
		.RayGenerationShaderRecord = RayGenerationShaderTable,
		.MissShaderTable = MissShaderTable,
		.HitGroupTable = HitGroupShaderTable,
		.Width = Width,
		.Height = Height,
		.Depth = 1
	};

	CommandList.DispatchRays(&desc);
	CommandList.UAVBarrier(m_RenderTarget->pResource.Get());
}