#include "pch.h"
#include "Picking.h"

#include "RendererRegistry.h"

#include <ResourceUploadBatch.h>

using namespace DirectX;

namespace
{
	// Symbols
	constexpr LPCWSTR RayGeneration = L"RayGeneration";
	constexpr LPCWSTR Miss = L"Miss";
	constexpr LPCWSTR ClosestHit = L"ClosestHit";

	// HitGroup Exports
	constexpr LPCWSTR HitGroupExport = L"Default";

	ShaderIdentifier RayGenerationSID;
	ShaderIdentifier MissSID;
	ShaderIdentifier DefaultSID;
}

void Picking::Create()
{
	auto& RenderDevice = RenderDevice::Instance();

	GlobalRS = RenderDevice.CreateRootSignature([](RootSignatureBuilder& Builder)
	{
		Builder.AddRootCBVParameter(RootCBV(0, 0)); // g_SystemConstants		b0 | space0

		Builder.AddRootSRVParameter(RootSRV(0, 0));	// Scene					t0 | space0

		Builder.AddRootUAVParameter(RootUAV(0, 0));	// PickingResult,			u0 | space0
	});

	RTPSO = RenderDevice.CreateRaytracingPipelineState([&](RaytracingPipelineStateBuilder& Builder)
	{
		const Library* pRaytraceLibrary = &Libraries::Picking;

		Builder.AddLibrary(pRaytraceLibrary,
			{
				RayGeneration,
				Miss,
				ClosestHit
			});

		Builder.AddHitGroup(HitGroupExport, nullptr, ClosestHit, nullptr);

		Builder.SetGlobalRootSignature(&GlobalRS);

		Builder.SetRaytracingShaderConfig(sizeof(int), SizeOfBuiltInTriangleIntersectionAttributes);
		Builder.SetRaytracingPipelineConfig(1);
	});

	RayGenerationSID = RTPSO.GetShaderIdentifier(L"RayGeneration");
	MissSID = RTPSO.GetShaderIdentifier(L"Miss");
	DefaultSID = RTPSO.GetShaderIdentifier(L"Default");

	ResourceUploadBatch Uploader(RenderDevice.Device);

	Uploader.Begin(D3D12_COMMAND_LIST_TYPE_COPY);

	D3D12MA::ALLOCATION_DESC AllocDesc = {};
	AllocDesc.Flags = D3D12MA::ALLOCATION_FLAG_COMMITTED;
	AllocDesc.HeapType = D3D12_HEAP_TYPE_DEFAULT;

	// Ray Generation Shader Table
	{
		ShaderTable<void>::Record Record = {};
		Record.ShaderIdentifier = RayGenerationSID;

		RayGenerationShaderTable.AddShaderRecord(Record);

		UINT64 shaderTableSizeInBytes = RayGenerationShaderTable.GetSizeInBytes();

		SharedGraphicsResource rayGenSBTUpload = RenderDevice.Device.GraphicsMemory()->Allocate(shaderTableSizeInBytes);
		m_RayGenerationShaderTable = RenderDevice.CreateBuffer(&AllocDesc, shaderTableSizeInBytes);

		RayGenerationShaderTable.AssociateResource(m_RayGenerationShaderTable->pResource.Get());
		RayGenerationShaderTable.Generate(static_cast<BYTE*>(rayGenSBTUpload.Memory()));

		Uploader.Upload(RayGenerationShaderTable.Resource(), rayGenSBTUpload);
	}

	// Miss Shader Table
	{
		ShaderTable<void>::Record Record = {};
		Record.ShaderIdentifier = MissSID;

		MissShaderTable.AddShaderRecord(Record);

		UINT64 shaderTableSizeInBytes = MissShaderTable.GetSizeInBytes();

		SharedGraphicsResource missSBTUpload = RenderDevice.Device.GraphicsMemory()->Allocate(shaderTableSizeInBytes);
		m_MissShaderTable = RenderDevice.CreateBuffer(&AllocDesc, shaderTableSizeInBytes);

		MissShaderTable.AssociateResource(m_MissShaderTable->pResource.Get());
		MissShaderTable.Generate(static_cast<BYTE*>(missSBTUpload.Memory()));

		Uploader.Upload(m_MissShaderTable->pResource.Get(), missSBTUpload);
	}

	auto finish = Uploader.End(RenderDevice.CopyQueue);
	finish.wait();

	m_HitGroupShaderTable = RenderDevice.CreateBuffer(&AllocDesc, HitGroupShaderTable.GetStrideInBytes() * Scene::MAX_INSTANCE_SUPPORTED);

	AllocDesc = {};
	AllocDesc.HeapType = D3D12_HEAP_TYPE_DEFAULT;

	PickingResult = RenderDevice.CreateBuffer(&AllocDesc, sizeof(int),
		D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS, 0, D3D12_RESOURCE_STATE_UNORDERED_ACCESS);

	AllocDesc.HeapType = D3D12_HEAP_TYPE_READBACK;

	PickingReadback = RenderDevice.CreateBuffer(&AllocDesc, sizeof(int),
		D3D12_RESOURCE_FLAG_NONE, 0, D3D12_RESOURCE_STATE_COPY_DEST);

	HitGroupShaderTable.Reserve(Scene::MAX_INSTANCE_SUPPORTED);
	HitGroupShaderTable.AssociateResource(m_HitGroupShaderTable->pResource.Get());

	Entities.reserve(Scene::MAX_INSTANCE_SUPPORTED);
}

void Picking::UpdateShaderTable(const RaytracingAccelerationStructure& RaytracingAccelerationStructure, CommandList& CommandList)
{
	auto& RenderDevice = RenderDevice::Instance();

	HitGroupShaderTable.Clear();
	Entities.clear();
	for (auto [i, MeshRenderer] : enumerate(RaytracingAccelerationStructure.MeshRenderers))
	{
		ShaderTable<void>::Record Record = {};
		Record.ShaderIdentifier = DefaultSID;

		HitGroupShaderTable.AddShaderRecord(Record);

		Entity Entity(MeshRenderer->Handle, MeshRenderer->pScene);
		Entities.push_back(Entity);
	}

	UINT64 shaderTableSizeInBytes = HitGroupShaderTable.GetSizeInBytes();

	GraphicsResource HitGroupUpload = RenderDevice.Device.GraphicsMemory()->Allocate(shaderTableSizeInBytes);

	HitGroupShaderTable.Generate(static_cast<BYTE*>(HitGroupUpload.Memory()));

	CommandList.TransitionBarrier(m_HitGroupShaderTable->pResource.Get(), D3D12_RESOURCE_STATE_COPY_DEST);
	CommandList->CopyBufferRegion(m_HitGroupShaderTable->pResource.Get(), 0, HitGroupUpload.Resource(), HitGroupUpload.ResourceOffset(), HitGroupUpload.Size());
	CommandList.TransitionBarrier(m_HitGroupShaderTable->pResource.Get(), D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE);
}

void Picking::ShootPickingRay(D3D12_GPU_VIRTUAL_ADDRESS SystemConstants,
	const RaytracingAccelerationStructure& Scene, CommandList& CommandList)
{
	CommandList->SetPipelineState1(RTPSO);
	CommandList->SetComputeRootSignature(GlobalRS);
	CommandList->SetComputeRootConstantBufferView(0, SystemConstants);
	CommandList->SetComputeRootShaderResourceView(1, Scene);
	CommandList->SetComputeRootUnorderedAccessView(2, PickingResult->pResource->GetGPUVirtualAddress());

	D3D12_DISPATCH_RAYS_DESC Desc =
	{
		.RayGenerationShaderRecord = RayGenerationShaderTable,
		.MissShaderTable = MissShaderTable,
		.HitGroupTable = HitGroupShaderTable,
		.Width = 1,
		.Height = 1,
		.Depth = 1
	};

	CommandList.DispatchRays(&Desc);

	CommandList.TransitionBarrier(PickingResult->pResource.Get(), D3D12_RESOURCE_STATE_COPY_SOURCE);
	CommandList->CopyBufferRegion(PickingReadback->pResource.Get(), 0, PickingResult->pResource.Get(), 0, sizeof(int));
	CommandList.TransitionBarrier(PickingResult->pResource.Get(), D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE);
}

std::optional<Entity> Picking::GetSelectedEntity()
{
	INT InstanceID = -1;
	INT* pHostMemory = nullptr;
	if (SUCCEEDED(PickingReadback->pResource->Map(0, nullptr, reinterpret_cast<void**>(&pHostMemory))))
	{
		InstanceID = pHostMemory[0];
	}
	PickingReadback->pResource->Unmap(0, nullptr);

	if (InstanceID != -1 && InstanceID < Entities.size())
	{
		return Entities[InstanceID];
	}

	return {};
}