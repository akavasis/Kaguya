#include "pch.h"
#include "AmbientOcclusion.h"

#include "Graphics/Scene/Scene.h"
#include "Graphics/GpuScene.h"
#include "Graphics/RenderGraph.h"
#include "Graphics/RendererRegistry.h"

AmbientOcclusion::AmbientOcclusion(UINT Width, UINT Height)
	: RenderPass("Ambient Occlusion",
		{ Width, Height, RendererFormats::HDRBufferFormat })
{

}

void AmbientOcclusion::InitializePipeline(RenderDevice* pRenderDevice)
{
	RaytracingPSOs::AmbientOcclusion = pRenderDevice->CreateRaytracingPipelineState([&](RaytracingPipelineStateProxy& proxy)
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

		const Library* pRaytraceLibrary = &Libraries::AmbientOcclusion;

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

		proxy.SetRaytracingShaderConfig(SizeOfHLSLBooleanType, SizeOfBuiltInTriangleIntersectionAttributes);
		proxy.SetRaytracingPipelineConfig(2);
	});
}

void AmbientOcclusion::ScheduleResource(ResourceScheduler* pResourceScheduler, RenderGraph* pRenderGraph)
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

void AmbientOcclusion::InitializeScene(GpuScene* pGpuScene, RenderDevice* pRenderDevice)
{
	this->pGpuScene = pGpuScene;

	RaytracingPipelineState* pRaytracingPipelineState = pRenderDevice->GetRaytracingPSO(RaytracingPSOs::AmbientOcclusion);

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
	PIXMarker(RenderContext->GetD3DCommandList(), L"Ambient Occlusion");

	struct AmbientOcclusionData
	{
		float AORadius;
		int NumAORaysPerPixel;

		int InputWorldPositionIndex;
		int InputWorldNormalIndex;
		int OutputIndex;
	} Data;

	Data.AORadius = Settings.AORadius;
	Data.NumAORaysPerPixel = Settings.NumAORaysPerPixel;

	// TODO: UPDATE THIS VALUE
	//Data.InputWorldPositionIndex = RenderContext.GetShaderResourceView(pRaytraceGBufferRenderPass->Resources[RaytraceGBuffer::EResources::WorldPosition]).HeapIndex;
	//Data.InputWorldNormalIndex = RenderContext.GetShaderResourceView(pRaytraceGBufferRenderPass->Resources[RaytraceGBuffer::EResources::WorldNormal]).HeapIndex;
	Data.OutputIndex = RenderContext.GetUnorderedAccessView(Resources[EResources::RenderTarget]).HeapIndex;
	RenderContext.UpdateRenderPassData<AmbientOcclusionData>(Data);

	RenderContext.TransitionBarrier(Resources[EResources::RenderTarget], DeviceResource::State::UnorderedAccess);

	RenderContext.SetPipelineState(RaytracingPSOs::AmbientOcclusion);
	RenderContext.SetRootShaderResourceView(0, pGpuScene->GetRTTLASResourceHandle());
	RenderContext.SetRootShaderResourceView(1, pGpuScene->GetVertexBufferHandle());
	RenderContext.SetRootShaderResourceView(2, pGpuScene->GetIndexBufferHandle());
	RenderContext.SetRootShaderResourceView(3, pGpuScene->GetMeshTable());
	RenderContext.SetRootShaderResourceView(4, pGpuScene->GetMaterialTableHandle());

	RenderContext.DispatchRays(
		m_RayGenerationShaderTable,
		m_MissShaderTable,
		m_HitGroupShaderTable,
		Properties.Width,
		Properties.Height);

	RenderContext.UAVBarrier(Resources[EResources::RenderTarget]);
}

void AmbientOcclusion::StateRefresh()
{

}