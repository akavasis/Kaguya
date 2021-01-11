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

	static constexpr UINT MinimumSamples = 1;
	static constexpr UINT MaximumSamples = 16;

	static constexpr UINT MinimumDepth = 1;
	static constexpr UINT MaximumDepth = 16;
}

Pathtracing::Pathtracing(UINT Width, UINT Height)
	: RenderPass("Path tracing", { Width, Height }, NumResources)
{
	UseRayTracing = true;
}

void Pathtracing::InitializePipeline(RenderDevice* pRenderDevice)
{
	Resources[RenderTarget]		= pRenderDevice->InitializeRenderResourceHandle(RenderResourceType::Texture, "Render Pass[" + Name + "]: " + "RenderTarget");
	m_RayGenerationShaderTable	= pRenderDevice->InitializeRenderResourceHandle(RenderResourceType::Buffer, "Render Pass[" + Name + "]: " + "Ray Generation Shader Table");
	m_MissShaderTable			= pRenderDevice->InitializeRenderResourceHandle(RenderResourceType::Buffer, "Render Pass[" + Name + "]: " + "Miss Shader Table");
	m_HitGroupShaderTable		= pRenderDevice->InitializeRenderResourceHandle(RenderResourceType::Buffer, "Render Pass[" + Name + "]: " + "Hit Group Shader Table");

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

	// Ray Generation Shader Table
	{
		ShaderTable<void> shaderTable;
		shaderTable.AddShaderRecord(pRaytracingPipelineState->GetShaderIdentifier(L"RayGeneration"));

		UINT64 shaderTableSizeInBytes, stride;
		shaderTable.ComputeMemoryRequirements(&shaderTableSizeInBytes);
		stride = shaderTable.GetShaderRecordStride();

		pRenderDevice->CreateBuffer(m_RayGenerationShaderTable, [shaderTableSizeInBytes, stride](BufferProxy& proxy)
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

		UINT64 shaderTableSizeInBytes, stride;
		shaderTable.ComputeMemoryRequirements(&shaderTableSizeInBytes);
		stride = shaderTable.GetShaderRecordStride();

		pRenderDevice->CreateBuffer(m_MissShaderTable, [shaderTableSizeInBytes, stride](BufferProxy& proxy)
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
		struct RootArgument
		{
			D3D12_GPU_VIRTUAL_ADDRESS VertexBuffer;
			D3D12_GPU_VIRTUAL_ADDRESS IndexBuffer;
		};

		ShaderTable<RootArgument> shaderTable;

		ShaderIdentifier hitGroupSID = pRaytracingPipelineState->GetShaderIdentifier(L"Default");

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
			shaderTable.AddShaderRecord(hitGroupSID, argument);
		}

		UINT64 shaderTableSizeInBytes, stride;
		shaderTable.ComputeMemoryRequirements(&shaderTableSizeInBytes);
		stride = shaderTable.GetShaderRecordStride();

		pRenderDevice->CreateBuffer(m_HitGroupShaderTable, [shaderTableSizeInBytes, stride](BufferProxy& proxy)
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

	RenderContext.DispatchRays
	(
		m_RayGenerationShaderTable,
		m_MissShaderTable,
		m_HitGroupShaderTable,
		Properties.Width, Properties.Height
	);
}

void Pathtracing::StateRefresh()
{

}