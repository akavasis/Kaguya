#include "pch.h"
#include "Shading.h"
#include <Core/RenderSystem.h>

#include "GBuffer.h"

Shading::Shading(UINT Width, UINT Height)
	: RenderPass("Shading", 
		{ Width, Height, RendererFormats::HDRBufferFormat })
{
	pGpuScene = nullptr;
}

void Shading::InitializePipeline(RenderDevice* pRenderDevice)
{
	RaytracingPSOs::Shading = pRenderDevice->CreateRaytracingPipelineState([&](RaytracingPipelineStateProxy& proxy)
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

void Shading::ScheduleResource(ResourceScheduler* pResourceScheduler)
{
	for (UINT i = 0; i < NumResources; ++i)
	{
		pResourceScheduler->AllocateTexture(DeviceResource::Type::Texture2D, [&](DeviceTextureProxy& proxy)
		{
			proxy.SetFormat(Properties.Format);
			proxy.SetWidth(Properties.Width);
			proxy.SetHeight(Properties.Height);
			proxy.BindFlags = DeviceResource::BindFlags::UnorderedAccess;
			proxy.InitialState = DeviceResource::State::UnorderedAccess;
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

		m_RayGenerationShaderTable = pRenderDevice->CreateDeviceBuffer([shaderTableSizeInBytes, stride](DeviceBufferProxy& proxy)
		{
			proxy.SetSizeInBytes(shaderTableSizeInBytes);
			proxy.SetStride(stride);
			proxy.SetCpuAccess(DeviceBuffer::CpuAccess::Write);
		});

		DeviceBuffer* pShaderTableBuffer = pRenderDevice->GetBuffer(m_RayGenerationShaderTable);
		shaderTable.Generate(pShaderTableBuffer);
	}

	// Miss Shader Table
	{
		ShaderTable<void> shaderTable;
		shaderTable.AddShaderRecord(pRaytracingPipelineState->GetShaderIdentifier(L"ShadowMiss"));

		UINT64 shaderTableSizeInBytes; UINT stride;
		shaderTable.ComputeMemoryRequirements(&shaderTableSizeInBytes);
		stride = shaderTable.GetShaderRecordStride();

		m_MissShaderTable = pRenderDevice->CreateDeviceBuffer([shaderTableSizeInBytes, stride](DeviceBufferProxy& proxy)
		{
			proxy.SetSizeInBytes(shaderTableSizeInBytes);
			proxy.SetStride(stride);
			proxy.SetCpuAccess(DeviceBuffer::CpuAccess::Write);
		});

		DeviceBuffer* pShaderTableBuffer = pRenderDevice->GetBuffer(m_MissShaderTable);
		shaderTable.Generate(pShaderTableBuffer);
	}

	// Hit Group Shader Table
	{
		ShaderTable<void> shaderTable;
		shaderTable.AddShaderRecord(pRaytracingPipelineState->GetShaderIdentifier(L"Default"));

		UINT64 shaderTableSizeInBytes; UINT stride;
		shaderTable.ComputeMemoryRequirements(&shaderTableSizeInBytes);
		stride = shaderTable.GetShaderRecordStride();

		m_HitGroupShaderTable = pRenderDevice->CreateDeviceBuffer([shaderTableSizeInBytes, stride](DeviceBufferProxy& proxy)
		{
			proxy.SetSizeInBytes(shaderTableSizeInBytes);
			proxy.SetStride(stride);
			proxy.SetCpuAccess(DeviceBuffer::CpuAccess::Write);
		});

		DeviceBuffer* pShaderTableBuffer = pRenderDevice->GetBuffer(m_HitGroupShaderTable);
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

	struct ShadingData
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
	
	RenderContext.UpdateRenderPassData<ShadingData>(Data);

	RenderContext.TransitionBarrier(Resources[EResources::AnalyticUnshadowed], DeviceResource::State::UnorderedAccess);
	RenderContext.TransitionBarrier(Resources[EResources::StochasticUnshadowed], DeviceResource::State::UnorderedAccess);
	RenderContext.TransitionBarrier(Resources[EResources::StochasticShadowed], DeviceResource::State::UnorderedAccess);

	RenderContext.SetPipelineState(RaytracingPSOs::Shading);
	RenderContext.SetRootShaderResourceView(0, pGpuScene->GetRTTLASResourceHandle());
	RenderContext.SetRootShaderResourceView(1, pGpuScene->GetVertexBufferHandle());
	RenderContext.SetRootShaderResourceView(2, pGpuScene->GetIndexBufferHandle());
	RenderContext.SetRootShaderResourceView(3, pGpuScene->GetGeometryInfoTableHandle());
	RenderContext.SetRootShaderResourceView(4, pGpuScene->GetLightTableHandle());
	RenderContext.SetRootShaderResourceView(5, pGpuScene->GetMaterialTableHandle());

	RenderContext.DispatchRays(
		m_RayGenerationShaderTable,
		m_MissShaderTable,
		m_HitGroupShaderTable,
		Properties.Width,
		Properties.Height);

	RenderContext.UAVBarrier(Resources[EResources::AnalyticUnshadowed]);
	RenderContext.UAVBarrier(Resources[EResources::StochasticUnshadowed]);
	RenderContext.UAVBarrier(Resources[EResources::StochasticShadowed]);
}

void Shading::StateRefresh()
{

}