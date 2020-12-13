#include "pch.h"
#include "Shading.h"

#include "Graphics/Scene/Scene.h"
#include "Graphics/GpuScene.h"
#include "Graphics/RenderGraph.h"
#include "Graphics/RendererRegistry.h"

#include "GBuffer.h"

namespace
{
	// Symbols
	const LPCWSTR RayGeneration		= L"RayGeneration";
	const LPCWSTR ShadowMiss		= L"ShadowMiss";
	const LPCWSTR ShadowClosestHit	= L"ShadowClosestHit";

	// HitGroup Exports
	const LPCWSTR HitGroupExport	= L"Default";
}

Shading::Shading(UINT Width, UINT Height)
	: RenderPass("Shading",  { Width, Height }, NumResources)
{
	UseRayTracing = true;

	pGpuScene = nullptr;
}

void Shading::InitializePipeline(RenderDevice* pRenderDevice)
{
	Resources[AnalyticUnshadowed]	= pRenderDevice->InitializeRenderResourceHandle(RenderResourceType::Texture, "Render Pass[" + Name + "]: " + "AnalyticUnshadowed");
	Resources[StochasticUnshadowed] = pRenderDevice->InitializeRenderResourceHandle(RenderResourceType::Texture, "Render Pass[" + Name + "]: " + "StochasticUnshadowed");
	Resources[StochasticShadowed]	= pRenderDevice->InitializeRenderResourceHandle(RenderResourceType::Texture, "Render Pass[" + Name + "]: " + "StochasticShadowed");
	m_RayGenerationShaderTable		= pRenderDevice->InitializeRenderResourceHandle(RenderResourceType::Buffer, "Render Pass[" + Name + "]: " + "Ray Generation Shader Table");
	m_MissShaderTable				= pRenderDevice->InitializeRenderResourceHandle(RenderResourceType::Buffer, "Render Pass[" + Name + "]: " + "Miss Shader Table");
	m_HitGroupShaderTable			= pRenderDevice->InitializeRenderResourceHandle(RenderResourceType::Buffer, "Render Pass[" + Name + "]: " + "Hit Group Shader Table");

	pRenderDevice->CreateRaytracingPipelineState(RaytracingPSOs::Shading, [&](RaytracingPipelineStateBuilder& Builder)
	{
		const Library* pRaytraceLibrary = &Libraries::Shading;

		Builder.AddLibrary(pRaytraceLibrary,
			{
				RayGeneration,
				ShadowMiss,
				ShadowClosestHit
			});

		Builder.AddHitGroup(HitGroupExport, nullptr, ShadowClosestHit, nullptr);

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
				ShadowMiss,
				ShadowClosestHit
			});

		Builder.SetGlobalRootSignature(pGlobalRootSignature);

		Builder.SetRaytracingShaderConfig(SizeOfHLSLBooleanType, SizeOfBuiltInTriangleIntersectionAttributes);
		Builder.SetRaytracingPipelineConfig(1);
	});
}

void Shading::ScheduleResource(ResourceScheduler* pResourceScheduler, RenderGraph* pRenderGraph)
{
	for (UINT i = 0; i < NumResources; ++i)
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
}

void Shading::InitializeScene(GpuScene* pGpuScene, RenderDevice* pRenderDevice)
{
	this->pGpuScene = pGpuScene;

	RaytracingPipelineState* pRaytracingPipelineState = pRenderDevice->GetRaytracingPipelineState(RaytracingPSOs::Shading);

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

void Shading::RenderGui()
{

}

void Shading::Execute(RenderContext& RenderContext, RenderGraph* pRenderGraph)
{
	auto pGBufferRenderPass = pRenderGraph->GetRenderPass<GBuffer>();

	struct RenderPassData
	{
		// Input textures
		int Albedo;
		int Normal;
		int TypeAndIndex;
		int Depth;

		int LTC_LUT_DisneyDiffuse_InverseMatrix;
		int LTC_LUT_DisneyDiffuse_Terms;
		int LTC_LUT_GGX_InverseMatrix;
		int LTC_LUT_GGX_Terms;
		int BlueNoise;

		// Output textures
		int AnalyticUnshadowed;
		int StochasticUnshadowed;
		int StochasticShadowed;
	} g_RenderPassData = {};

	g_RenderPassData.Albedo									= RenderContext.GetShaderResourceView(pGBufferRenderPass->Resources[GBuffer::EResources::Albedo]).HeapIndex;
	g_RenderPassData.Normal									= RenderContext.GetShaderResourceView(pGBufferRenderPass->Resources[GBuffer::EResources::Normal]).HeapIndex;
	g_RenderPassData.TypeAndIndex							= RenderContext.GetShaderResourceView(pGBufferRenderPass->Resources[GBuffer::EResources::TypeAndIndex]).HeapIndex;
	g_RenderPassData.Depth									= RenderContext.GetShaderResourceView(pGBufferRenderPass->Resources[GBuffer::EResources::Depth]).HeapIndex;

	g_RenderPassData.LTC_LUT_DisneyDiffuse_InverseMatrix	= RenderContext.GetShaderResourceView(pGpuScene->TextureManager.GetLTC_LUT_DisneyDiffuse_InverseMatrixTexture()).HeapIndex;
	g_RenderPassData.LTC_LUT_DisneyDiffuse_Terms			= RenderContext.GetShaderResourceView(pGpuScene->TextureManager.GetLTC_LUT_DisneyDiffuse_TermsTexture()).HeapIndex;
	g_RenderPassData.LTC_LUT_GGX_InverseMatrix				= RenderContext.GetShaderResourceView(pGpuScene->TextureManager.GetLTC_LUT_GGX_InverseMatrixTexture()).HeapIndex;
	g_RenderPassData.LTC_LUT_GGX_Terms						= RenderContext.GetShaderResourceView(pGpuScene->TextureManager.GetLTC_LUT_GGX_TermsTexture()).HeapIndex;
	g_RenderPassData.BlueNoise								= RenderContext.GetShaderResourceView(pGpuScene->TextureManager.GetBlueNoise()).HeapIndex;
	
	g_RenderPassData.AnalyticUnshadowed						= RenderContext.GetUnorderedAccessView(Resources[EResources::AnalyticUnshadowed]).HeapIndex;
	g_RenderPassData.StochasticUnshadowed					= RenderContext.GetUnorderedAccessView(Resources[EResources::StochasticUnshadowed]).HeapIndex;
	g_RenderPassData.StochasticShadowed						= RenderContext.GetUnorderedAccessView(Resources[EResources::StochasticShadowed]).HeapIndex;
	
	RenderContext.UpdateRenderPassData<RenderPassData>(g_RenderPassData);

	RenderContext.SetPipelineState(RaytracingPSOs::Shading);
	RenderContext.SetRootShaderResourceView(0, pGpuScene->GetTopLevelAccelerationStructure());
	RenderContext.SetRootShaderResourceView(1, pGpuScene->GetMeshTable());
	RenderContext.SetRootShaderResourceView(2, pGpuScene->GetLightTable());
	RenderContext.SetRootShaderResourceView(3, pGpuScene->GetMaterialTable());

	RenderContext.DispatchRays(
		m_RayGenerationShaderTable,
		m_MissShaderTable,
		m_HitGroupShaderTable,
		Properties.Width,
		Properties.Height);
}

void Shading::StateRefresh()
{

}