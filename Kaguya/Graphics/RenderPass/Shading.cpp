#include "pch.h"
#include "Shading.h"

#include "Graphics/Scene/Scene.h"
#include "Graphics/GpuScene.h"
#include "Graphics/RenderGraph.h"
#include "Graphics/RendererRegistry.h"

#include "GBuffer.h"

Shading::Shading(UINT Width, UINT Height)
	: RenderPass("Shading", 
		{ Width, Height, RendererFormats::HDRBufferFormat },
		NumResources)
{
	UseRayTracing = true;

	pGpuScene = nullptr;
}

void Shading::InitializePipeline(RenderDevice* pRenderDevice)
{
	Resources[AnalyticUnshadowed]	= pRenderDevice->InitializeRenderResourceHandle(RenderResourceType::Texture, "Render Pass[" + Name + "]: " + "AnalyticUnshadowed");
	Resources[StochasticUnshadowed] = pRenderDevice->InitializeRenderResourceHandle(RenderResourceType::Texture, "Render Pass[" + Name + "]: " + "StochasticUnshadowed");
	Resources[StochasticShadowed]	= pRenderDevice->InitializeRenderResourceHandle(RenderResourceType::Texture, "Render Pass[" + Name + "]: " + "StochasticShadowed");
	m_RayGenerationShaderTable = pRenderDevice->InitializeRenderResourceHandle(RenderResourceType::Buffer, "Render Pass[" + Name + "]: " + "Ray Generation Shader Table");
	m_MissShaderTable = pRenderDevice->InitializeRenderResourceHandle(RenderResourceType::Buffer, "Render Pass[" + Name + "]: " + "Miss Shader Table");
	m_HitGroupShaderTable = pRenderDevice->InitializeRenderResourceHandle(RenderResourceType::Buffer, "Render Pass[" + Name + "]: " + "Hit Group Shader Table");

	pRenderDevice->CreateRaytracingPipelineState(RaytracingPSOs::Shading, [&](RaytracingPipelineStateProxy& proxy)
	{
		enum Symbols
		{
			RayGeneration,
			ShadowMiss,
			ShadowClosestHit,
			NumSymbols
		};

		const LPCWSTR symbols[NumSymbols] =
		{
			ENUM_TO_LSTR(RayGeneration),
			ENUM_TO_LSTR(ShadowMiss),
			ENUM_TO_LSTR(ShadowClosestHit),
		};

		enum HitGroups
		{
			Default,
			NumHitGroups
		};

		const LPCWSTR hitGroups[NumHitGroups] =
		{
			ENUM_TO_LSTR(Default)
		};

		const Library* pRaytraceLibrary = &Libraries::Shading;

		proxy.AddLibrary(pRaytraceLibrary,
			{
				symbols[RayGeneration],
				symbols[ShadowMiss],
				symbols[ShadowClosestHit]
			});

		proxy.AddHitGroup(hitGroups[Default], nullptr, symbols[ShadowClosestHit], nullptr);

		RootSignature* pGlobalRootSignature = pRenderDevice->GetRootSignature(RootSignatures::Raytracing::Global);
		RootSignature* pEmptyLocalRootSignature = pRenderDevice->GetRootSignature(RootSignatures::Raytracing::EmptyLocal);

		// The following section associates the root signature to each shader. Note
		// that we can explicitly show that some shaders share the same root signature
		// (eg. Miss and ShadowMiss). Note that the hit shaders are now only referred
		// to as hit groups, meaning that the underlying intersection, any-hit and
		// closest-hit shaders share the same root signature.
		proxy.AddRootSignatureAssociation(pEmptyLocalRootSignature,
			{
				symbols[RayGeneration],
				symbols[ShadowMiss],
				symbols[ShadowClosestHit],
			});

		proxy.SetGlobalRootSignature(pGlobalRootSignature);

		proxy.SetRaytracingShaderConfig(SizeOfHLSLBooleanType, SizeOfBuiltInTriangleIntersectionAttributes);
		proxy.SetRaytracingPipelineConfig(1);
	});
}

void Shading::ScheduleResource(ResourceScheduler* pResourceScheduler, RenderGraph* pRenderGraph)
{
	for (UINT i = 0; i < NumResources; ++i)
	{
		pResourceScheduler->AllocateTexture(Resource::Type::Texture2D, [&](TextureProxy& proxy)
		{
			proxy.SetFormat(Properties.Format);
			proxy.SetWidth(Properties.Width);
			proxy.SetHeight(Properties.Height);
			proxy.BindFlags = Resource::BindFlags::UnorderedAccess;
			proxy.InitialState = Resource::State::UnorderedAccess;
		});
	}
}

void Shading::InitializeScene(GpuScene* pGpuScene, RenderDevice* pRenderDevice)
{
	this->pGpuScene = pGpuScene;

	RaytracingPipelineState* pRaytracingPipelineState = pRenderDevice->GetRaytracingPSO(RaytracingPSOs::Shading);

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
	PIXMarker(RenderContext->GetD3DCommandList(), L"Shading");

	auto pGBufferRenderPass = pRenderGraph->GetRenderPass<GBuffer>();

	struct RenderPassData
	{
		int Albedo;
		int Normal;
		int TypeAndIndex;
		int Depth;

		int LTC_LUT_DisneyDiffuse_InverseMatrix;
		int LTC_LUT_DisneyDiffuse_Terms;
		int LTC_LUT_GGX_InverseMatrix;
		int LTC_LUT_GGX_Terms;
		int BlueNoise;

		int AnalyticUnshadowed;
		int StochasticUnshadowed;
		int StochasticShadowed;
	} Data;

	Data.Albedo									= RenderContext.GetShaderResourceView(pGBufferRenderPass->Resources[GBuffer::EResources::Albedo]).HeapIndex;
	Data.Normal									= RenderContext.GetShaderResourceView(pGBufferRenderPass->Resources[GBuffer::EResources::Normal]).HeapIndex;
	Data.TypeAndIndex							= RenderContext.GetShaderResourceView(pGBufferRenderPass->Resources[GBuffer::EResources::TypeAndIndex]).HeapIndex;
	Data.Depth									= RenderContext.GetShaderResourceView(pGBufferRenderPass->Resources[GBuffer::EResources::Depth]).HeapIndex;

	Data.LTC_LUT_DisneyDiffuse_InverseMatrix	= RenderContext.GetShaderResourceView(pGpuScene->GpuTextureAllocator.GetLTC_LUT_DisneyDiffuse_InverseMatrixTexture()).HeapIndex;
	Data.LTC_LUT_DisneyDiffuse_Terms			= RenderContext.GetShaderResourceView(pGpuScene->GpuTextureAllocator.GetLTC_LUT_DisneyDiffuse_TermsTexture()).HeapIndex;
	Data.LTC_LUT_GGX_InverseMatrix				= RenderContext.GetShaderResourceView(pGpuScene->GpuTextureAllocator.GetLTC_LUT_GGX_InverseMatrixTexture()).HeapIndex;
	Data.LTC_LUT_GGX_Terms						= RenderContext.GetShaderResourceView(pGpuScene->GpuTextureAllocator.GetLTC_LUT_GGX_TermsTexture()).HeapIndex;
	Data.BlueNoise								= RenderContext.GetShaderResourceView(pGpuScene->GpuTextureAllocator.GetBlueNoise()).HeapIndex;
	
	Data.AnalyticUnshadowed						= RenderContext.GetUnorderedAccessView(Resources[EResources::AnalyticUnshadowed]).HeapIndex;
	Data.StochasticUnshadowed					= RenderContext.GetUnorderedAccessView(Resources[EResources::StochasticUnshadowed]).HeapIndex;
	Data.StochasticShadowed						= RenderContext.GetUnorderedAccessView(Resources[EResources::StochasticShadowed]).HeapIndex;
	
	RenderContext.UpdateRenderPassData<RenderPassData>(Data);

	RenderContext.SetPipelineState(RaytracingPSOs::Shading);
	RenderContext.SetRootShaderResourceView(0, pGpuScene->GetTopLevelAccelerationStructure());
	RenderContext.SetRootShaderResourceView(1, pGpuScene->GetVertexBuffer());
	RenderContext.SetRootShaderResourceView(2, pGpuScene->GetIndexBuffer());
	RenderContext.SetRootShaderResourceView(3, pGpuScene->GetMeshTable());
	RenderContext.SetRootShaderResourceView(4, pGpuScene->GetLightTable());
	RenderContext.SetRootShaderResourceView(5, pGpuScene->GetMaterialTable());

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