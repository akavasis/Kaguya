#include "pch.h"
#include "AmbientOcclusion.h"

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
}

AmbientOcclusion::AmbientOcclusion(UINT Width, UINT Height)
	: RenderPass("Ambient Occlusion",
		{ Width, Height },
		NumResources)
{
	UseRayTracing = true;
}

void AmbientOcclusion::InitializePipeline(RenderDevice* pRenderDevice)
{
	Resources[RenderTarget]		= pRenderDevice->InitializeRenderResourceHandle(RenderResourceType::Texture, "Render Pass[" + Name + "]: " + "RenderTarget");
	m_RayGenerationShaderTable	= pRenderDevice->InitializeRenderResourceHandle(RenderResourceType::Buffer, "Render Pass[" + Name + "]: " + "Ray Generation Shader Table");
	m_MissShaderTable			= pRenderDevice->InitializeRenderResourceHandle(RenderResourceType::Buffer, "Render Pass[" + Name + "]: " + "Miss Shader Table");
	m_HitGroupShaderTable		= pRenderDevice->InitializeRenderResourceHandle(RenderResourceType::Buffer, "Render Pass[" + Name + "]: " + "Hit Group Shader Table");

	pRenderDevice->CreateRaytracingPipelineState(RaytracingPSOs::AmbientOcclusion, [&](RaytracingPipelineStateBuilder& Builder)
	{
		const Library* pRaytraceLibrary = &Libraries::AmbientOcclusion;

		Builder.AddLibrary(pRaytraceLibrary,
			{
				RayGeneration,
				Miss,
				ClosestHit
			});

		Builder.AddHitGroup(HitGroupExport, nullptr, ClosestHit, nullptr);

		RootSignature* pGlobalRootSignature = pRenderDevice->GetRootSignature(RootSignatures::Raytracing::Global);

		Builder.SetGlobalRootSignature(pGlobalRootSignature);

		Builder.SetRaytracingShaderConfig(SizeOfHLSLBooleanType, SizeOfBuiltInTriangleIntersectionAttributes);
		Builder.SetRaytracingPipelineConfig(2);
	});
}

void AmbientOcclusion::ScheduleResource(ResourceScheduler* pResourceScheduler, RenderGraph* pRenderGraph)
{
	pResourceScheduler->AllocateTexture(Resource::Type::Texture2D, [&](TextureProxy& proxy)
	{
		proxy.SetFormat(DXGI_FORMAT_R32G32B32A32_FLOAT);
		proxy.SetWidth(Properties.Width);
		proxy.SetHeight(Properties.Height);
		proxy.BindFlags = Resource::Flags::UnorderedAccess;
		proxy.InitialState = Resource::State::UnorderedAccess;
	});
}

void AmbientOcclusion::InitializeScene(GpuScene* pGpuScene, RenderDevice* pRenderDevice)
{
	this->pGpuScene = pGpuScene;

	RaytracingPipelineState* pRaytracingPipelineState = pRenderDevice->GetRaytracingPipelineState(RaytracingPSOs::AmbientOcclusion);

	// Ray Generation Shader Table
	{
		ShaderTable<void> shaderTable;
		shaderTable.AddShaderRecord(pRaytracingPipelineState->GetShaderIdentifier(L"RayGeneration"));

		UINT64 shaderTableSizeInBytes; UINT stride;
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

		UINT64 shaderTableSizeInBytes; UINT stride;
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
		ShaderTable<void> shaderTable;

		ShaderIdentifier hitGroupSID = pRaytracingPipelineState->GetShaderIdentifier(L"Default");

		for (const auto& meshInstance : pGpuScene->pScene->MeshInstances)
		{
			shaderTable.AddShaderRecord(hitGroupSID);
		}

		UINT64 shaderTableSizeInBytes; UINT stride;
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

void AmbientOcclusion::RenderGui()
{
	if (ImGui::TreeNode(Name.data()))
	{
		int Dirty = 0;
		if (ImGui::Button("Restore Defaults"))
		{
			Settings = SSettings();
			Refresh = true;
		}
		Dirty |= (int)ImGui::DragFloat("AO Radius", &Settings.AORadius, Settings.AORadius * 0.01f, 1e-4f, 1e38f);
		Dirty |= (int)ImGui::SliderInt("Num AO Rays Per Pixel", &Settings.NumAORaysPerPixel, 1, 64);
		
		if (Dirty) Refresh = true;
		ImGui::TreePop();
	}
}

void AmbientOcclusion::Execute(RenderContext& RenderContext, RenderGraph* pRenderGraph)
{
	struct RenderPassData
	{
		float AORadius;
		int NumAORaysPerPixel;

		int InputWorldPositionIndex;
		int InputWorldNormalIndex;
		int OutputIndex;
	} g_RenderPassData;

	g_RenderPassData.AORadius = Settings.AORadius;
	g_RenderPassData.NumAORaysPerPixel = Settings.NumAORaysPerPixel;

	// TODO: UPDATE THIS VALUE
	//Data.InputWorldPositionIndex = RenderContext.GetShaderResourceView(pRaytraceGBufferRenderPass->Resources[RaytraceGBuffer::EResources::WorldPosition]).HeapIndex;
	//Data.InputWorldNormalIndex = RenderContext.GetShaderResourceView(pRaytraceGBufferRenderPass->Resources[RaytraceGBuffer::EResources::WorldNormal]).HeapIndex;
	g_RenderPassData.OutputIndex = RenderContext.GetUnorderedAccessView(Resources[EResources::RenderTarget]).HeapIndex;
	RenderContext.UpdateRenderPassData<RenderPassData>(g_RenderPassData);

	RenderContext.SetPipelineState(RaytracingPSOs::AmbientOcclusion);
	RenderContext.SetRootShaderResourceView(0, pGpuScene->GetTopLevelAccelerationStructure());
	//RenderContext.SetRootShaderResourceView(1, pGpuScene->GetVertexBuffer());
	//RenderContext.SetRootShaderResourceView(2, pGpuScene->GetIndexBuffer());
	RenderContext.SetRootShaderResourceView(3, pGpuScene->GetMeshTable());
	RenderContext.SetRootShaderResourceView(4, pGpuScene->GetMaterialTable());

	RenderContext.DispatchRays
	(
		m_RayGenerationShaderTable,
		m_MissShaderTable,
		m_HitGroupShaderTable,
		Properties.Width,
		Properties.Height
	);
}

void AmbientOcclusion::StateRefresh()
{

}