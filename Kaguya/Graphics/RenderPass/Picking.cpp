#include "pch.h"
#include "Picking.h"

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

Picking::Picking()
	: RenderPass("Picking", { 0, 0 }, 0)
{
	UseRayTracing = true;

	pGpuScene = nullptr;
}

void Picking::InitializePipeline(RenderDevice* pRenderDevice)
{
	m_RayGenerationShaderTable	= pRenderDevice->InitializeRenderResourceHandle(RenderResourceType::Buffer, "Render Pass[" + Name + "]: " + "Ray Generation Shader Table");
	m_MissShaderTable			= pRenderDevice->InitializeRenderResourceHandle(RenderResourceType::Buffer, "Render Pass[" + Name + "]: " + "Miss Shader Table");
	m_HitGroupShaderTable		= pRenderDevice->InitializeRenderResourceHandle(RenderResourceType::Buffer, "Render Pass[" + Name + "]: " + "Hit Group Shader Table");

	pRenderDevice->CreateRaytracingPipelineState(RaytracingPSOs::Picking, [&](RaytracingPipelineStateBuilder& Builder)
	{
		const Library* pRaytraceLibrary = &Libraries::Picking;

		Builder.AddLibrary(pRaytraceLibrary,
			{
				RayGeneration,
				Miss,
				ClosestHit
			});

		Builder.AddHitGroup(HitGroupExport, nullptr, ClosestHit, nullptr);

		RootSignature* pGlobalRootSignature = pRenderDevice->GetRootSignature(RootSignatures::Raytracing::Global);
		RootSignature* pEmptyLocalRootSignature = pRenderDevice->GetRootSignature(RootSignatures::Raytracing::EmptyLocal);

		// The following section associates the root signature to each shader. Note
		// that we can explicitly show that some shaders share the same root signature
		// (eg. Miss and ShadowMiss). Note that the hit shaders are now only referred
		// to as hit groups, meaning that the underlying intersection, any-hit and
		// closest-hit shaders share the same root signature.
		Builder.AddRootSignatureAssociation(pEmptyLocalRootSignature,
			{
				RayGeneration,
				Miss,
				HitGroupExport
			});

		Builder.SetGlobalRootSignature(pGlobalRootSignature);

		Builder.SetRaytracingShaderConfig(SizeOfHLSLBooleanType, SizeOfBuiltInTriangleIntersectionAttributes);
		Builder.SetRaytracingPipelineConfig(1);
	});
}

void Picking::ScheduleResource(ResourceScheduler* pResourceScheduler, RenderGraph* pRenderGraph)
{

}

void Picking::InitializeScene(GpuScene* pGpuScene, RenderDevice* pRenderDevice)
{
	this->pGpuScene = pGpuScene;

	RaytracingPipelineState* pRaytracingPipelineState = pRenderDevice->GetRaytracingPipelineState(RaytracingPSOs::Picking);

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
		shaderTable.AddShaderRecord(pRaytracingPipelineState->GetShaderIdentifier(L"ShadowMiss"));

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
		shaderTable.AddShaderRecord(pRaytracingPipelineState->GetShaderIdentifier(L"Default"));

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

void Picking::RenderGui()
{

}

void Picking::Execute(RenderContext& RenderContext, RenderGraph* pRenderGraph)
{

}

void Picking::StateRefresh()
{

}
