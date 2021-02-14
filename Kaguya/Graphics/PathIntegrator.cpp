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
	constexpr LPCWSTR ClosestHit = L"ClosestHit";

	// HitGroup Exports
	constexpr LPCWSTR HitGroupExport = L"Default";

	ShaderIdentifier RayGenerationSID;
	ShaderIdentifier MissSID;
	ShaderIdentifier DefaultSID;

	struct Settings
	{
		static constexpr UINT MinimumSamples = 1;
		static constexpr UINT MaximumSamples = 16;

		static constexpr UINT MinimumDepth = 1;
		static constexpr UINT MaximumDepth = 16;

		UINT NumSamplesPerPixel;
		UINT MaxDepth;
		UINT NumAccumulatedSamples;

		Settings()
		{
			NumSamplesPerPixel = 4;
			MaxDepth = 6;
			NumAccumulatedSamples = 0;
		}
	} g_Settings;
}

void PathIntegrator::Create()
{
	auto& RenderDevice = RenderDevice::Instance();

	UAV = RenderDevice.AllocateUnorderedAccessView();
	SRV = RenderDevice.AllocateShaderResourceView();

	GlobalRS = RenderDevice.CreateRootSignature([](RootSignatureBuilder& Builder)
	{
		Builder.AddRootCBVParameter(RootCBV(0, 0)); // g_SystemConstants		b0 | space0
		Builder.AddRootCBVParameter(RootCBV(1, 0)); // g_RenderPassData			b1 | space0

		Builder.AddRootSRVParameter(RootSRV(0, 0));	// Scene					t0 | space0
		Builder.AddRootSRVParameter(RootSRV(1, 0));	// Materials				t1 | space0

		Builder.AddStaticSampler(0, D3D12_FILTER_MIN_MAG_MIP_LINEAR, D3D12_TEXTURE_ADDRESS_MODE_WRAP, 16);	// SamplerLinearWrap	s0 | space0;
		Builder.AddStaticSampler(1, D3D12_FILTER_MIN_MAG_MIP_LINEAR, D3D12_TEXTURE_ADDRESS_MODE_CLAMP, 16);	// SamplerLinearClamp	s1 | space0;
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
				ClosestHit
			});

		Builder.AddHitGroup(HitGroupExport, nullptr, ClosestHit, nullptr);

		Builder.AddRootSignatureAssociation(&LocalHitGroupRS,
			{
				HitGroupExport
			});

		Builder.SetGlobalRootSignature(&GlobalRS);

		Builder.SetRaytracingShaderConfig(12 * sizeof(float) + 2 * sizeof(unsigned int), SizeOfBuiltInTriangleIntersectionAttributes);
		Builder.SetRaytracingPipelineConfig(1);
	});

	RayGenerationSID = RTPSO.GetShaderIdentifier(L"RayGeneration");
	MissSID = RTPSO.GetShaderIdentifier(L"Miss");
	DefaultSID = RTPSO.GetShaderIdentifier(L"Default");

	ResourceUploadBatch Uploader(RenderDevice.Device);

	Uploader.Begin(D3D12_COMMAND_LIST_TYPE_COPY);

	D3D12MA::ALLOCATION_DESC AllocDesc = {};
	AllocDesc.Flags = D3D12MA::ALLOCATION_FLAG_COMMITTED;
	AllocDesc.HeapType = D3D12_HEAP_TYPE_DEFAULT;

	// Ray Generation Shader Table
	{
		ShaderTable<void>::Record Record = {};
		Record.ShaderIdentifier = RayGenerationSID;

		RayGenerationShaderTable.AddShaderRecord(Record);

		UINT64 shaderTableSizeInBytes = RayGenerationShaderTable.GetSizeInBytes();

		SharedGraphicsResource rayGenSBTUpload = RenderDevice.Device.GraphicsMemory()->Allocate(shaderTableSizeInBytes);
		m_RayGenerationShaderTable = RenderDevice.CreateBuffer(&AllocDesc, shaderTableSizeInBytes);

		RayGenerationShaderTable.AssociateResource(m_RayGenerationShaderTable->pResource.Get());
		RayGenerationShaderTable.Generate(static_cast<BYTE*>(rayGenSBTUpload.Memory()));

		Uploader.Upload(RayGenerationShaderTable.Resource(), rayGenSBTUpload);
	}

	// Miss Shader Table
	{
		ShaderTable<void>::Record Record = {};
		Record.ShaderIdentifier = MissSID;

		MissShaderTable.AddShaderRecord(Record);

		UINT64 shaderTableSizeInBytes = MissShaderTable.GetSizeInBytes();

		SharedGraphicsResource missSBTUpload = RenderDevice.Device.GraphicsMemory()->Allocate(shaderTableSizeInBytes);
		m_MissShaderTable = RenderDevice.CreateBuffer(&AllocDesc, shaderTableSizeInBytes);

		MissShaderTable.AssociateResource(m_MissShaderTable->pResource.Get());
		MissShaderTable.Generate(static_cast<BYTE*>(missSBTUpload.Memory()));

		Uploader.Upload(m_MissShaderTable->pResource.Get(), missSBTUpload);
	}

	auto finish = Uploader.End(RenderDevice.CopyQueue);
	finish.wait();

	m_HitGroupShaderTable = RenderDevice.CreateBuffer(&AllocDesc, HitGroupShaderTable.GetStrideInBytes() * Scene::MAX_INSTANCE_SUPPORTED);

	HitGroupShaderTable.Reserve(Scene::MAX_INSTANCE_SUPPORTED);
	HitGroupShaderTable.AssociateResource(m_HitGroupShaderTable->pResource.Get());
}

void PathIntegrator::SetResolution(UINT Width, UINT Height)
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

	auto ResourceDesc = CD3DX12_RESOURCE_DESC::Tex2D(DXGI_FORMAT_R32G32B32A32_FLOAT, Width, Height);
	ResourceDesc.MipLevels = 1;
	ResourceDesc.Flags = D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS;

	m_RenderTarget = RenderDevice.CreateResource(&AllocationDesc,
		&ResourceDesc, D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE, nullptr);

	RenderDevice.CreateUnorderedAccessView(m_RenderTarget->pResource.Get(), UAV);
	RenderDevice.CreateShaderResourceView(m_RenderTarget->pResource.Get(), SRV);

	Reset();
}

void PathIntegrator::RenderGui()
{
	if (ImGui::TreeNode("Path Integrator"))
	{
		if (ImGui::Button("Restore Defaults"))
		{
			g_Settings = Settings();
		}

		bool Dirty = false;
		Dirty |= ImGui::SliderScalar("Num Samples Per Pixel", ImGuiDataType_U32, &g_Settings.NumSamplesPerPixel, &Settings::MinimumSamples, &Settings::MaximumSamples);
		Dirty |= ImGui::SliderScalar("Max Depth", ImGuiDataType_U32, &g_Settings.MaxDepth, &Settings::MinimumDepth, &Settings::MaximumDepth);
		ImGui::Text("Num Samples Accumulated: %u", g_Settings.NumAccumulatedSamples);

		if (Dirty)
		{
			Reset();
		}

		ImGui::TreePop();
	}
}

void PathIntegrator::Reset()
{
	g_Settings.NumAccumulatedSamples = 0;
}

void PathIntegrator::UpdateShaderTable(const RaytracingAccelerationStructure& RaytracingAccelerationStructure, CommandList& CommandList)
{
	auto& RenderDevice = RenderDevice::Instance();

	HitGroupShaderTable.Clear();
	for (auto [i, MeshRenderer] : enumerate(RaytracingAccelerationStructure.MeshRenderers))
	{
		auto MeshFilter = MeshRenderer->pMeshFilter;

		const auto& VB = MeshFilter->Mesh->VertexResource->pResource;
		const auto& IB = MeshFilter->Mesh->IndexResource->pResource;

		ShaderTable<RootArgument>::Record Record = {};
		Record.ShaderIdentifier = DefaultSID;
		Record.RootArguments = 
		{
			.MaterialIndex = (UINT)i,
			.Padding = 0,
			.VertexBuffer = VB->GetGPUVirtualAddress(),
			.IndexBuffer = IB->GetGPUVirtualAddress()
		};

		HitGroupShaderTable.AddShaderRecord(Record);
	}

	UINT64 shaderTableSizeInBytes = HitGroupShaderTable.GetSizeInBytes();

	GraphicsResource HitGroupUpload = RenderDevice.Device.GraphicsMemory()->Allocate(shaderTableSizeInBytes);

	HitGroupShaderTable.Generate(static_cast<BYTE*>(HitGroupUpload.Memory()));

	CommandList.TransitionBarrier(m_HitGroupShaderTable->pResource.Get(), D3D12_RESOURCE_STATE_COPY_DEST);
	CommandList->CopyBufferRegion(m_HitGroupShaderTable->pResource.Get(), 0, HitGroupUpload.Resource(), HitGroupUpload.ResourceOffset(), HitGroupUpload.Size());
	CommandList.TransitionBarrier(m_HitGroupShaderTable->pResource.Get(), D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE);
}

void PathIntegrator::Render(D3D12_GPU_VIRTUAL_ADDRESS SystemConstants,
	const RaytracingAccelerationStructure& RaytracingAccelerationStructure,
	D3D12_GPU_VIRTUAL_ADDRESS Materials,
	CommandList& CommandList)
{
	auto& RenderDevice = RenderDevice::Instance();

	if (g_Settings.NumAccumulatedSamples == 10000)
	{
		LOG_INFO("Reached 10000");
		return;
	}

	struct RenderPassData
	{
		uint NumSamplesPerPixel;
		uint MaxDepth;
		uint NumAccumulatedSamples;

		uint RenderTarget;
	} g_RenderPassData = {};

	g_RenderPassData.NumSamplesPerPixel = g_Settings.NumSamplesPerPixel;
	g_RenderPassData.MaxDepth = g_Settings.MaxDepth;
	g_RenderPassData.NumAccumulatedSamples = g_Settings.NumAccumulatedSamples++;

	g_RenderPassData.RenderTarget = UAV.Index;

	GraphicsResource ConstantBuffer = RenderDevice.Device.GraphicsMemory()->AllocateConstant(g_RenderPassData);

	CommandList->SetPipelineState1(RTPSO);
	CommandList->SetComputeRootSignature(GlobalRS);
	CommandList->SetComputeRootConstantBufferView(0, SystemConstants);
	CommandList->SetComputeRootConstantBufferView(1, ConstantBuffer.GpuAddress());
	CommandList->SetComputeRootShaderResourceView(2, RaytracingAccelerationStructure);
	CommandList->SetComputeRootShaderResourceView(3, Materials);

	RenderDevice.BindDescriptorTable(PipelineState::Type::Compute, GlobalRS, CommandList);

	D3D12_DISPATCH_RAYS_DESC Desc =
	{
		.RayGenerationShaderRecord = RayGenerationShaderTable,
		.MissShaderTable = MissShaderTable,
		.HitGroupTable = HitGroupShaderTable,
		.Width = Width,
		.Height = Height,
		.Depth = 1
	};

	CommandList.DispatchRays(&Desc);
	CommandList.UAVBarrier(m_RenderTarget->pResource.Get());
}