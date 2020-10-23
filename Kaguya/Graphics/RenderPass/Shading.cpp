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
			NumSymbols
		};

		const LPCWSTR symbols[NumSymbols] =
		{
			ENUM_TO_LSTR(RayGeneration)
		};

		const Library* pRaytraceLibrary = &Libraries::Shading;

		proxy.AddLibrary(pRaytraceLibrary,
			{
				symbols[RayGeneration]
			});

		RootSignature* pGlobalRootSignature = pRenderDevice->GetRootSignature(RootSignatures::Raytracing::Global);
		RootSignature* pEmptyLocalRootSignature = pRenderDevice->GetRootSignature(RootSignatures::Raytracing::EmptyLocal);

		// The following section associates the root signature to each shader. Note
		// that we can explicitly show that some shaders share the same root signature
		// (eg. Miss and ShadowMiss). Note that the hit shaders are now only referred
		// to as hit groups, meaning that the underlying intersection, any-hit and
		// closest-hit shaders share the same root signature.
		proxy.AddRootSignatureAssociation(pEmptyLocalRootSignature,
			{
				symbols[RayGeneration]
			});

		proxy.SetGlobalRootSignature(pGlobalRootSignature);

		proxy.SetRaytracingShaderConfig(SizeOfHLSLBooleanType, SizeOfBuiltInTriangleIntersectionAttributes);
		proxy.SetRaytracingPipelineConfig(8);
	});
}

void Shading::ScheduleResource(ResourceScheduler* pResourceScheduler)
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
		int Position;
		int Normal;
		int Albedo;
		int TypeAndIndex;
		int DepthStencil;

		int LTCLUT1;
		int LTCLUT2;

		int RenderTarget;
	} Data;

	Data.Position = RenderContext.GetShaderResourceView(pGBufferRenderPass->Resources[GBuffer::EResources::Position]).HeapIndex;
	Data.Normal = RenderContext.GetShaderResourceView(pGBufferRenderPass->Resources[GBuffer::EResources::Normal]).HeapIndex;
	Data.Albedo = RenderContext.GetShaderResourceView(pGBufferRenderPass->Resources[GBuffer::EResources::Albedo]).HeapIndex;
	Data.TypeAndIndex = RenderContext.GetShaderResourceView(pGBufferRenderPass->Resources[GBuffer::EResources::TypeAndIndex]).HeapIndex;
	Data.DepthStencil = RenderContext.GetShaderResourceView(pGBufferRenderPass->Resources[GBuffer::EResources::DepthStencil]).HeapIndex;

	Data.LTCLUT1 = RenderContext.GetShaderResourceView(pGpuScene->GpuTextureAllocator.SystemReservedTextures[GpuTextureAllocator::SystemReservedTextures::LTCLUT1]).HeapIndex;
	Data.LTCLUT2 = RenderContext.GetShaderResourceView(pGpuScene->GpuTextureAllocator.SystemReservedTextures[GpuTextureAllocator::SystemReservedTextures::LTCLUT2]).HeapIndex;
	
	Data.RenderTarget = RenderContext.GetUnorderedAccessView(Resources[EResources::RenderTarget]).HeapIndex;
	
	RenderContext.UpdateRenderPassData<ShadingData>(Data);

	RenderContext.TransitionBarrier(Resources[EResources::RenderTarget], DeviceResource::State::UnorderedAccess);

	RenderContext.SetPipelineState(RaytracingPSOs::Shading);
	RenderContext.SetRootShaderResourceView(0, pGpuScene->GetRTTLASResourceHandle());
	RenderContext.SetRootShaderResourceView(1, pGpuScene->GetVertexBufferHandle());
	RenderContext.SetRootShaderResourceView(2, pGpuScene->GetIndexBufferHandle());
	RenderContext.SetRootShaderResourceView(3, pGpuScene->GetGeometryInfoTableHandle());
	RenderContext.SetRootShaderResourceView(4, pGpuScene->GetLightTableHandle());
	RenderContext.SetRootShaderResourceView(5, pGpuScene->GetMaterialTableHandle());

	RenderContext.DispatchRays(
		m_RayGenerationShaderTable,
		RenderResourceHandle(),
		RenderResourceHandle(),
		Properties.Width,
		Properties.Height);

	RenderContext.UAVBarrier(Resources[EResources::RenderTarget]);
}

void Shading::StateRefresh()
{

}