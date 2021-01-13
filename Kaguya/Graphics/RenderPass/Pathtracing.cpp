#include "pch.h"
#include "Pathtracing.h"

#include "Graphics/Scene/Scene.h"
#include "Graphics/GpuScene.h"
#include "Graphics/RenderGraph.h"
#include "Graphics/RendererRegistry.h"

namespace
{
	// Symbols
	const LPCWSTR RayGeneration		= L"RayGeneration";
	const LPCWSTR Miss				= L"Miss";
	const LPCWSTR ClosestHit		= L"ClosestHit";

	// HitGroup Exports
	const LPCWSTR HitGroupExport	= L"Default";

	const UINT MinimumSamples = 1;
	const UINT MaximumSamples = 16;

	const UINT MinimumDepth = 1;
	const UINT MaximumDepth = 16;

	ShaderIdentifier RayGenerationSID;
	ShaderIdentifier MissSID;
	ShaderIdentifier DefaultSID;
}

Pathtracing::Pathtracing(UINT Width, UINT Height)
	: RenderPass("Path tracing", { Width, Height }, NumResources)
{
	UseRayTracing = true;

	HitGroupShaderTable.Reserve(Scene::MAX_MESH_INSTANCE_SUPPORTED);
}

Pathtracing::~Pathtracing()
{
	SafeRelease(m_HitGroupShaderTableAllocation);
	SafeRelease(m_MissShaderTableAllocation);
	SafeRelease(m_RayGenerationShaderTableAllocation);
}

void Pathtracing::InitializePipeline(RenderDevice* pRenderDevice)
{
	Resources[RenderTarget]		= pRenderDevice->InitializeRenderResourceHandle(RenderResourceType::Texture, "Render Pass[" + Name + "]: " + "RenderTarget");

	pRenderDevice->CreateRaytracingPipelineState(RaytracingPSOs::Pathtracing, [&](RaytracingPipelineStateBuilder& Builder)
	{
		const Library* pRaytraceLibrary = &Libraries::Pathtracing;

		Builder.AddLibrary(pRaytraceLibrary,
			{
				RayGeneration,
				Miss,
				ClosestHit
			});

		Builder.AddHitGroup(HitGroupExport, nullptr, ClosestHit, nullptr);

		auto pGlobalRootSignature = pRenderDevice->GetRootSignature(RootSignatures::Raytracing::Global);
		auto pLocalRootSignature = pRenderDevice->GetRootSignature(RootSignatures::Raytracing::Local::Default);

		Builder.AddRootSignatureAssociation(pLocalRootSignature,
			{
				HitGroupExport
			});

		Builder.SetGlobalRootSignature(pGlobalRootSignature);

		Builder.SetRaytracingShaderConfig(12 * sizeof(float) + 2 * sizeof(unsigned int), SizeOfBuiltInTriangleIntersectionAttributes);
		Builder.SetRaytracingPipelineConfig(1);
	});
}

void Pathtracing::ScheduleResource(ResourceScheduler* pResourceScheduler, RenderGraph* pRenderGraph)
{
	pResourceScheduler->AllocateTexture(Resource::Type::Texture2D, [=](TextureProxy& proxy)
	{
		proxy.SetFormat(DXGI_FORMAT_R32G32B32A32_FLOAT);
		proxy.SetWidth(Properties.Width);
		proxy.SetHeight(Properties.Height);
		proxy.BindFlags = Resource::Flags::UnorderedAccess;
		proxy.InitialState = Resource::State::UnorderedAccess;
	});
}

void Pathtracing::InitializeScene(GpuScene* pGpuScene, RenderDevice* pRenderDevice)
{
	this->pGpuScene = pGpuScene;

	RaytracingPipelineState* pRaytracingPipelineState = pRenderDevice->GetRaytracingPipelineState(RaytracingPSOs::Pathtracing);
	RayGenerationSID = pRaytracingPipelineState->GetShaderIdentifier(L"RayGeneration");
	MissSID = pRaytracingPipelineState->GetShaderIdentifier(L"Miss");
	DefaultSID = pRaytracingPipelineState->GetShaderIdentifier(L"Default");

	// Ray Generation Shader Table
	{
		ShaderTable<void> ShaderTable;
		ShaderTable.AddShaderRecord(RayGenerationSID);

		UINT64 shaderTableSizeInBytes, stride;
		ShaderTable.ComputeMemoryRequirements(&shaderTableSizeInBytes);
		stride = ShaderTable.GetShaderRecordStride();

		D3D12MA::ALLOCATION_DESC AllocDesc = {};
		AllocDesc.HeapType = D3D12_HEAP_TYPE_UPLOAD;
		D3D12_RESOURCE_DESC Desc = CD3DX12_RESOURCE_DESC::Buffer(shaderTableSizeInBytes, D3D12_RESOURCE_FLAG_NONE);

		pRenderDevice->Device.Allocator()->CreateResource(&AllocDesc, &Desc,
			D3D12_RESOURCE_STATE_GENERIC_READ, nullptr,
			&m_RayGenerationShaderTableAllocation, IID_PPV_ARGS(m_RayGenerationShaderTable.ReleaseAndGetAddressOf()));

		ShaderTable.Generate(m_RayGenerationShaderTable.Get());

		RayGenerationShaderRecord.StartAddress = m_RayGenerationShaderTable->GetGPUVirtualAddress();
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
		D3D12_RESOURCE_DESC Desc = CD3DX12_RESOURCE_DESC::Buffer(shaderTableSizeInBytes, D3D12_RESOURCE_FLAG_NONE);

		pRenderDevice->Device.Allocator()->CreateResource(&AllocDesc, &Desc,
			D3D12_RESOURCE_STATE_GENERIC_READ, nullptr,
			&m_MissShaderTableAllocation, IID_PPV_ARGS(m_MissShaderTable.ReleaseAndGetAddressOf()));

		ShaderTable.Generate(m_MissShaderTable.Get());

		MissShaderTable.StartAddress = m_MissShaderTable->GetGPUVirtualAddress();
		MissShaderTable.SizeInBytes = shaderTableSizeInBytes;
		MissShaderTable.StrideInBytes = stride;
	}

	// Hit Group Shader Table
	{
		for (const auto& MeshInstance : pGpuScene->pScene->MeshInstances)
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
		}

		UINT64 shaderTableSizeInBytes, stride = HitGroupShaderTable.GetShaderRecordStride();
		shaderTableSizeInBytes = Scene::MAX_MESH_INSTANCE_SUPPORTED * stride;
		shaderTableSizeInBytes = Math::AlignUp<UINT64>(shaderTableSizeInBytes, D3D12_RAYTRACING_SHADER_TABLE_BYTE_ALIGNMENT);

		D3D12MA::ALLOCATION_DESC AllocDesc = {};
		AllocDesc.HeapType = D3D12_HEAP_TYPE_UPLOAD;
		D3D12_RESOURCE_DESC Desc = CD3DX12_RESOURCE_DESC::Buffer(shaderTableSizeInBytes, D3D12_RESOURCE_FLAG_NONE);

		pRenderDevice->Device.Allocator()->CreateResource(&AllocDesc, &Desc,
			D3D12_RESOURCE_STATE_GENERIC_READ, nullptr,
			&m_HitGroupShaderTableAllocation, IID_PPV_ARGS(m_HitGroupShaderTable.ReleaseAndGetAddressOf()));

		HitGroupShaderTable.Generate(m_HitGroupShaderTable.Get());

		HitGroupTable.StartAddress = m_HitGroupShaderTable->GetGPUVirtualAddress();
		HitGroupTable.SizeInBytes = shaderTableSizeInBytes;
		HitGroupTable.StrideInBytes = stride;
	}
}

void Pathtracing::RenderGui()
{
	if (ImGui::TreeNode(Name.data()))
	{
		if (ImGui::Button("Restore Defaults"))
		{
			settings = Settings();
			Refresh = true;
		}

		bool Dirty = false;
		Dirty |= ImGui::SliderScalar("Num Samples Per Pixel", ImGuiDataType_U32, &settings.NumSamplesPerPixel, &MinimumSamples, &MaximumSamples);
		Dirty |= ImGui::SliderScalar("Max Depth", ImGuiDataType_U32, &settings.MaxDepth, &MinimumDepth, &MaximumDepth);

		if (Dirty)
		{
			Refresh = true;
		}

		ImGui::TreePop();
	}
}

void Pathtracing::Execute(RenderContext& RenderContext, RenderGraph* pRenderGraph)
{
	HitGroupShaderTable.Clear();
	for (const auto& MeshInstance : pGpuScene->pScene->MeshInstances)
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

	HitGroupTable.SizeInBytes = pGpuScene->pScene->MeshInstances.size() * HitGroupShaderTable.GetShaderRecordStride();

	struct RenderPassData
	{
		uint NumSamplesPerPixel;
		uint MaxDepth;

		uint RenderTarget;
	} g_RenderPassData = {};

	g_RenderPassData.NumSamplesPerPixel = settings.NumSamplesPerPixel;
	g_RenderPassData.MaxDepth			= settings.MaxDepth;

	g_RenderPassData.RenderTarget		= RenderContext.GetUnorderedAccessView(Resources[EResources::RenderTarget]).HeapIndex;
	
	RenderContext.UpdateRenderPassData<RenderPassData>(g_RenderPassData);

	RenderContext.SetPipelineState(RaytracingPSOs::Pathtracing);
	RenderContext.SetRootShaderResourceView(0, pGpuScene->GetTopLevelAccelerationStructure());
	RenderContext.SetRootShaderResourceView(1, pGpuScene->GetMeshTable());
	RenderContext.SetRootShaderResourceView(2, pGpuScene->GetLightTable());
	RenderContext.SetRootShaderResourceView(3, pGpuScene->GetMaterialTable());

	D3D12_DISPATCH_RAYS_DESC Desc = {};
	Desc.RayGenerationShaderRecord = RayGenerationShaderRecord;

	Desc.MissShaderTable = MissShaderTable;

	Desc.HitGroupTable = HitGroupTable;

	Desc.Width = Properties.Width;
	Desc.Height = Properties.Height;
	Desc.Depth = 1;
	RenderContext->DispatchRays(&Desc);
}

void Pathtracing::StateRefresh()
{

}