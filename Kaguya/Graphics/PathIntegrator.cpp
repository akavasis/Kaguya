#include "pch.h"
#include "PathIntegrator.h"

#include "RendererRegistry.h"

namespace
{
	// Symbols
	const LPCWSTR RayGeneration = L"RayGeneration";
	const LPCWSTR Miss = L"Miss";
	const LPCWSTR ClosestHit = L"ClosestHit";

	// HitGroup Exports
	const LPCWSTR HitGroupExport = L"Default";

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

PathIntegrator::PathIntegrator()
{
	HitGroupShaderTable.Reserve(Scene::MAX_MESH_INSTANCE_SUPPORTED);
}

void PathIntegrator::Create(RenderDevice* pRenderDevice, RTScene* pRTScene)
{
	this->pRenderDevice = pRenderDevice;

	UAV = pRenderDevice->AllocateUnorderedAccessView();

	GlobalRS = pRenderDevice->CreateRootSignature([](RootSignatureBuilder& Builder)
	{
		Builder.AddRootSRVParameter(RootSRV(0, 0));	// BVH,						t0 | space0
		Builder.AddRootSRVParameter(RootSRV(1, 0));	// Meshes,					t1 | space0
		Builder.AddRootSRVParameter(RootSRV(2, 0));	// Lights,					t2 | space0
		Builder.AddRootSRVParameter(RootSRV(3, 0));	// Materials				t3 | space0

		Builder.AddStaticSampler(0, D3D12_FILTER_MIN_MAG_MIP_LINEAR, D3D12_TEXTURE_ADDRESS_MODE_WRAP, 16);	// SamplerLinearWrap	s0 | space0;
		Builder.AddStaticSampler(1, D3D12_FILTER_MIN_MAG_MIP_LINEAR, D3D12_TEXTURE_ADDRESS_MODE_CLAMP, 16);	// SamplerLinearClamp	s1 | space0;
	});

	LocalHitGroupRS = pRenderDevice->CreateRootSignature([](RootSignatureBuilder& Builder)
	{
		Builder.AddRootSRVParameter(RootSRV(0, 1));	// VertexBuffer,			t0 | space1
		Builder.AddRootSRVParameter(RootSRV(1, 1));	// IndexBuffer				t1 | space1

		Builder.SetAsLocalRootSignature();
	}, false);

	RTPSO = pRenderDevice->CreateRaytracingPipelineState([&](RaytracingPipelineStateBuilder& Builder)
	{
		const Library* pRaytraceLibrary = &Libraries::Pathtracing;

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

	// Ray Generation Shader Table
	{
		ShaderTable<void> ShaderTable;
		ShaderTable.AddShaderRecord(RayGenerationSID);

		UINT64 shaderTableSizeInBytes, stride;
		ShaderTable.ComputeMemoryRequirements(&shaderTableSizeInBytes);
		stride = ShaderTable.GetShaderRecordStride();

		D3D12MA::ALLOCATION_DESC AllocDesc = {};
		AllocDesc.HeapType = D3D12_HEAP_TYPE_UPLOAD;
		auto Desc = CD3DX12_RESOURCE_DESC::Buffer(shaderTableSizeInBytes, D3D12_RESOURCE_FLAG_NONE);

		m_RayGenerationShaderTable = pRenderDevice->CreateResource(&AllocDesc, &Desc,
			D3D12_RESOURCE_STATE_GENERIC_READ, nullptr);

		ShaderTable.Generate(m_RayGenerationShaderTable->pResource.Get());

		RayGenerationShaderRecord.StartAddress = m_RayGenerationShaderTable->pResource->GetGPUVirtualAddress();
		RayGenerationShaderRecord.SizeInBytes = shaderTableSizeInBytes;
	}

	// Miss Shader Table
	{
		ShaderTable<void> ShaderTable;
		ShaderTable.AddShaderRecord(MissSID);

		UINT64 shaderTableSizeInBytes, stride;
		ShaderTable.ComputeMemoryRequirements(&shaderTableSizeInBytes);
		stride = ShaderTable.GetShaderRecordStride();

		D3D12MA::ALLOCATION_DESC AllocDesc = {};
		AllocDesc.HeapType = D3D12_HEAP_TYPE_UPLOAD;
		auto Desc = CD3DX12_RESOURCE_DESC::Buffer(shaderTableSizeInBytes, D3D12_RESOURCE_FLAG_NONE);

		m_MissShaderTable = pRenderDevice->CreateResource(&AllocDesc, &Desc,
			D3D12_RESOURCE_STATE_GENERIC_READ, nullptr);

		ShaderTable.Generate(m_MissShaderTable->pResource.Get());

		MissShaderTable.StartAddress = m_MissShaderTable->pResource->GetGPUVirtualAddress();
		MissShaderTable.SizeInBytes = shaderTableSizeInBytes;
		MissShaderTable.StrideInBytes = stride;
	}

	// Hit Group Shader Table
	{
		/*for (const auto& MeshInstance : pGpuScene->pScene->MeshInstances)
		{
			const auto& Mesh = pGpuScene->pScene->Meshes[MeshInstance.MeshIndex];

			auto pVertexBuffer = pRenderDevice->GetBuffer(Mesh.VertexResource);
			auto pIndexBuffer = pRenderDevice->GetBuffer(Mesh.IndexResource);

			RootArgument argument =
			{
				.VertexBuffer = pVertexBuffer->GetGpuVirtualAddress(),
				.IndexBuffer = pIndexBuffer->GetGpuVirtualAddress()
			};
			HitGroupShaderTable.AddShaderRecord(DefaultSID, argument);
		}*/

		UINT64 shaderTableSizeInBytes, stride = HitGroupShaderTable.GetShaderRecordStride();
		shaderTableSizeInBytes = Scene::MAX_MESH_INSTANCE_SUPPORTED * stride;
		shaderTableSizeInBytes = Math::AlignUp<UINT64>(shaderTableSizeInBytes, D3D12_RAYTRACING_SHADER_TABLE_BYTE_ALIGNMENT);

		D3D12MA::ALLOCATION_DESC AllocDesc = {};
		AllocDesc.HeapType = D3D12_HEAP_TYPE_UPLOAD;
		D3D12_RESOURCE_DESC Desc = CD3DX12_RESOURCE_DESC::Buffer(shaderTableSizeInBytes, D3D12_RESOURCE_FLAG_NONE);

		m_HitGroupShaderTable = pRenderDevice->CreateResource(&AllocDesc, &Desc,
			D3D12_RESOURCE_STATE_GENERIC_READ, nullptr);

		HitGroupShaderTable.Generate(m_HitGroupShaderTable->pResource.Get());

		HitGroupTable.StartAddress = m_HitGroupShaderTable->pResource->GetGPUVirtualAddress();
		HitGroupTable.SizeInBytes = shaderTableSizeInBytes;
		HitGroupTable.StrideInBytes = stride;
	}
}

void PathIntegrator::SetResolution(UINT Width, UINT Height)
{
	D3D12MA::ALLOCATION_DESC AllocationDesc = {};
	AllocationDesc.Flags = D3D12MA::ALLOCATION_FLAG_COMMITTED;
	AllocationDesc.HeapType = D3D12_HEAP_TYPE_DEFAULT;

	auto ResourceDesc = CD3DX12_RESOURCE_DESC::Tex2D(DXGI_FORMAT_R32G32B32A32_FLOAT, Width, Height, 1, 1);

	m_RenderTarget = pRenderDevice->CreateResource(&AllocationDesc,
		&ResourceDesc, D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE, nullptr);

	pRenderDevice->CreateUnorderedAccessView(m_RenderTarget->pResource.Get(), UAV);
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
			g_Settings.NumAccumulatedSamples = 0;
		}

		ImGui::TreePop();
	}
}

void PathIntegrator::Render(CommandList& CommandList)
{
	HitGroupShaderTable.Clear();
	/*for (const auto& MeshInstance : pGpuScene->pScene->MeshInstances)
	{
		const auto& Mesh = pGpuScene->pScene->Meshes[MeshInstance.MeshIndex];

		auto pVertexBuffer = RenderContext.GetBuffer(Mesh.VertexResource);
		auto pIndexBuffer = RenderContext.GetBuffer(Mesh.IndexResource);

		RootArgument argument =
		{
			.VertexBuffer = pVertexBuffer->GetGpuVirtualAddress(),
			.IndexBuffer = pIndexBuffer->GetGpuVirtualAddress()
		};
		HitGroupShaderTable.AddShaderRecord(DefaultSID, argument);
	}

	HitGroupShaderTable.Generate(m_HitGroupShaderTable.Get());

	HitGroupTable.SizeInBytes = pGpuScene->pScene->MeshInstances.size() * HitGroupShaderTable.GetShaderRecordStride();*/

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

	//RenderContext.UpdateRenderPassData<RenderPassData>(g_RenderPassData);

	CommandList->SetPipelineState1(RTPSO);
	CommandList->SetComputeRootSignature(GlobalRS);
	//CommandList->SetComputeRootShaderResourceView(0, pGpuScene->GetTopLevelAccelerationStructure());
	//CommandList->SetComputeRootShaderResourceView(1, pGpuScene->GetMeshTable());
	//CommandList->SetComputeRootShaderResourceView(2, pGpuScene->GetLightTable());
	//CommandList->SetComputeRootShaderResourceView(3, pGpuScene->GetMaterialTable());

	D3D12_DISPATCH_RAYS_DESC Desc =
	{
		.RayGenerationShaderRecord = RayGenerationShaderRecord,
		.MissShaderTable = MissShaderTable,
		.HitGroupTable = HitGroupTable,
		.Width = Width,
		.Height = Height,
		.Depth = 1
	};

	CommandList.DispatchRays(&Desc);
}