#include "pch.h"
#include "Pathtracing.h"
#include <Core/RenderSystem.h>

Pathtracing::Pathtracing(UINT Width, UINT Height)
	: RenderPass("Path tracing",
		{ Width, Height, RendererFormats::HDRBufferFormat })
{

}

void Pathtracing::InitializePipeline(RenderDevice* pRenderDevice)
{
	RaytracingPSOs::Pathtracing = pRenderDevice->CreateRaytracingPipelineState([&](RaytracingPipelineStateProxy& proxy)
	{
		enum Symbols
		{
			RayGeneration,
			Miss,
			ClosestHit,
			NumSymbols
		};

		const LPCWSTR symbols[NumSymbols] =
		{
			ENUM_TO_LSTR(RayGeneration),
			ENUM_TO_LSTR(Miss),
			ENUM_TO_LSTR(ClosestHit)
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

		const Library* pRaytraceLibrary = &Libraries::Pathtracing;

		proxy.AddLibrary(pRaytraceLibrary,
			{
				symbols[RayGeneration],
				symbols[Miss],
				symbols[ClosestHit]
			});

		proxy.AddHitGroup(hitGroups[Default], nullptr, symbols[ClosestHit], nullptr);

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
				symbols[Miss],
				hitGroups[Default]
			});

		proxy.SetGlobalRootSignature(pGlobalRootSignature);

		proxy.SetRaytracingShaderConfig(6 * sizeof(float) + 2 * sizeof(unsigned int), SizeOfBuiltInTriangleIntersectionAttributes);
		proxy.SetRaytracingPipelineConfig(8);
	});
}

void Pathtracing::ScheduleResource(ResourceScheduler* pResourceScheduler, RenderGraph* pRenderGraph)
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

void Pathtracing::InitializeScene(GpuScene* pGpuScene, RenderDevice* pRenderDevice)
{
	this->pGpuScene = pGpuScene;

	RaytracingPipelineState* pRaytracingPipelineState = pRenderDevice->GetRaytracingPSO(RaytracingPSOs::Pathtracing);

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
		shaderTable.AddShaderRecord(pRaytracingPipelineState->GetShaderIdentifier(L"Miss"));

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

		ShaderIdentifier hitGroupSID = pRaytracingPipelineState->GetShaderIdentifier(L"Default");

		for (const auto& modelInstance : pGpuScene->pScene->ModelInstances)
		{
			for (const auto& meshInstance : modelInstance.MeshInstances)
			{
				shaderTable.AddShaderRecord(hitGroupSID);
			}
		}

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

void Pathtracing::RenderGui()
{
	if (ImGui::TreeNode("Path tracing"))
	{
		if (ImGui::Button("Restore Defaults"))
		{
			Settings = SSettings();
			Refresh = true;
		}

		int Dirty = 0;
		Dirty |= (int)ImGui::SliderInt("Num Samples Per Pixel", &Settings.NumSamplesPerPixel, 1, 10);
		Dirty |= (int)ImGui::SliderInt("Max Depth", &Settings.MaxDepth, 1, 7);
		
		if (Dirty) Refresh = true;
		ImGui::TreePop();
	}
}

void Pathtracing::Execute(RenderContext& RenderContext, RenderGraph* pRenderGraph)
{
	PIXMarker(RenderContext->GetD3DCommandList(), L"Path tracing");

	struct PathtracingData
	{
		uint OutputIndex;
	} Data;

	Data.OutputIndex = RenderContext.GetUnorderedAccessView(Resources[EResources::RenderTarget]).HeapIndex;
	RenderContext.UpdateRenderPassData<PathtracingData>(Data);

	RenderContext.TransitionBarrier(Resources[EResources::RenderTarget], DeviceResource::State::UnorderedAccess);

	RenderContext.SetPipelineState(RaytracingPSOs::Pathtracing);
	RenderContext.SetRootShaderResourceView(0, pGpuScene->GetRTTLASResourceHandle());
	RenderContext.SetRootShaderResourceView(1, pGpuScene->GetVertexBufferHandle());
	RenderContext.SetRootShaderResourceView(2, pGpuScene->GetIndexBufferHandle());
	RenderContext.SetRootShaderResourceView(3, pGpuScene->GetMeshTable());
	RenderContext.SetRootShaderResourceView(4, pGpuScene->GetLightTableHandle());
	RenderContext.SetRootShaderResourceView(5, pGpuScene->GetMaterialTableHandle());

	RenderContext.DispatchRays(
		m_RayGenerationShaderTable,
		m_MissShaderTable,
		m_HitGroupShaderTable,
		Properties.Width,
		Properties.Height);

	RenderContext.UAVBarrier(Resources[EResources::RenderTarget]);
}

void Pathtracing::StateRefresh()
{
}