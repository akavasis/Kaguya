#include "pch.h"
#include "Picking.h"

#include "Graphics/Scene/Scene.h"
#include "Graphics/GpuScene.h"
#include "Graphics/RenderGraph.h"
#include "Graphics/RendererRegistry.h"

#include "Core/Application.h"

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

INT Picking::GetInstanceID(RenderDevice* pRenderDevice)
{
	auto pPickingResultBuffer = pRenderDevice->GetBuffer(m_PickingResultReadBack);

	INT InstanceID;
	auto pCpuMemory = pPickingResultBuffer->Map();

	memcpy(&InstanceID, pCpuMemory, sizeof(int));

	pPickingResultBuffer->Unmap();
	return InstanceID;
}

void Picking::InitializePipeline(RenderDevice* pRenderDevice)
{
	m_PickingResult = pRenderDevice->InitializeRenderResourceHandle(RenderResourceType::Buffer, "Picking Result");
	m_PickingResultReadBack = pRenderDevice->InitializeRenderResourceHandle(RenderResourceType::Buffer, "Picking Result (Readback)");

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
		RootSignature* pLocalPickingRootSignature = pRenderDevice->GetRootSignature(RootSignatures::Raytracing::Local::Picking);
		RootSignature* pLocalEmptyRootSignature = pRenderDevice->GetRootSignature(RootSignatures::Raytracing::EmptyLocal);

		// The following section associates the root signature to each shader. Note
		// that we can explicitly show that some shaders share the same root signature
		// (eg. Miss and ShadowMiss). Note that the hit shaders are now only referred
		// to as hit groups, meaning that the underlying intersection, any-hit and
		// closest-hit shaders share the same root signature.
		Builder.SetGlobalRootSignature(pGlobalRootSignature);

		Builder.AddRootSignatureAssociation(pLocalPickingRootSignature,
			{
				RayGeneration
			});

		Builder.AddRootSignatureAssociation(pLocalEmptyRootSignature,
			{
				Miss,
				HitGroupExport
			});

		Builder.SetRaytracingShaderConfig(sizeof(int), SizeOfBuiltInTriangleIntersectionAttributes);
		Builder.SetRaytracingPipelineConfig(1);
	});
}

void Picking::ScheduleResource(ResourceScheduler* pResourceScheduler, RenderGraph* pRenderGraph)
{

}

void Picking::InitializeScene(GpuScene* pGpuScene, RenderDevice* pRenderDevice)
{
	// Picking result buffers
	{
		pRenderDevice->CreateBuffer(m_PickingResult, [](BufferProxy& proxy)
		{
			proxy.SetSizeInBytes(sizeof(int));
			proxy.SetStride(sizeof(int));
			proxy.BindFlags = Resource::Flags::UnorderedAccess;
			proxy.InitialState = Resource::State::NonPixelShaderResource;
		});

		pRenderDevice->CreateBuffer(m_PickingResultReadBack, [](BufferProxy& proxy)
		{
			proxy.SetSizeInBytes(sizeof(int));
			proxy.SetStride(sizeof(int));
			proxy.SetCpuAccess(Buffer::CpuAccess::Read);
		});
	}

	this->pGpuScene = pGpuScene;

	RaytracingPipelineState* pRaytracingPipelineState = pRenderDevice->GetRaytracingPipelineState(RaytracingPSOs::Picking);

	// Ray Generation Shader Table
	{
		struct RootArgument
		{
			D3D12_GPU_VIRTUAL_ADDRESS PickingResult;
		};

		auto PickingResult = pRenderDevice->GetBuffer(m_PickingResult);

		ShaderTable<RootArgument> shaderTable;
		RootArgument argument = 
		{
			.PickingResult = PickingResult->GetGpuVirtualAddress()
		};
		shaderTable.AddShaderRecord(pRaytracingPipelineState->GetShaderIdentifier(L"RayGeneration"), argument);

		UINT64 shaderTableSizeInBytes, stride;
		shaderTable.ComputeMemoryRequirements(&shaderTableSizeInBytes);
		stride = shaderTable.GetShaderRecordStride();

		pRenderDevice->CreateBuffer(m_RayGenerationShaderTable, [shaderTableSizeInBytes, stride](BufferProxy& proxy)
		{
			proxy.SetSizeInBytes(shaderTableSizeInBytes);
			proxy.SetStride(stride);
			proxy.SetCpuAccess(Buffer::CpuAccess::Write);
		});

		auto pShaderTableBuffer = pRenderDevice->GetBuffer(m_RayGenerationShaderTable);
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
		ShaderTable<void> shaderTable;

		ShaderIdentifier hitGroupSID = pRaytracingPipelineState->GetShaderIdentifier(L"Default");

		for (const auto& modelInstance : pGpuScene->pScene->ModelInstances)
		{
			for (const auto& meshInstance : modelInstance.MeshInstances)
			{
				shaderTable.AddShaderRecord(hitGroupSID);
			}
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

void Picking::RenderGui()
{

}

void Picking::Execute(RenderContext& RenderContext, RenderGraph* pRenderGraph)
{
	struct RenderPassData
	{
		uint2 MousePosition;
	} g_RenderPassData = {};

	g_RenderPassData.MousePosition.x = Application::pWindow->Mouse.X();
	g_RenderPassData.MousePosition.y = Application::pWindow->Mouse.Y();

	RenderContext.UpdateRenderPassData<RenderPassData>(g_RenderPassData);

	RenderContext.SetPipelineState(RaytracingPSOs::Picking);
	RenderContext.SetRootShaderResourceView(0, pGpuScene->GetTopLevelAccelerationStructure());
	RenderContext.SetRootShaderResourceView(1, pGpuScene->GetMeshTable());
	RenderContext.SetRootShaderResourceView(2, pGpuScene->GetLightTable());
	RenderContext.SetRootShaderResourceView(3, pGpuScene->GetMaterialTable());

	RenderContext.DispatchRays
	(
		m_RayGenerationShaderTable,
		m_MissShaderTable,
		m_HitGroupShaderTable,
		1,
		1
	);

	RenderContext.CopyResource(m_PickingResultReadBack, m_PickingResult);
}

void Picking::StateRefresh()
{

}