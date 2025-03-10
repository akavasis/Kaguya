#include "pch.h"
#include "RaytracingAccelerationStructure.h"

#include "RenderDevice.h"

using namespace DirectX;

void RaytracingAccelerationStructure::Create(UINT NumHitGroups)
{
	auto& RenderDevice = RenderDevice::Instance();

	this->NumHitGroups = NumHitGroups;

	D3D12MA::ALLOCATION_DESC Desc = {};
	Desc.HeapType = D3D12_HEAP_TYPE_UPLOAD;
	InstanceDescs = RenderDevice.CreateBuffer(&Desc, sizeof(D3D12_RAYTRACING_INSTANCE_DESC) * Scene::MAX_INSTANCE_SUPPORTED, D3D12_RESOURCE_FLAG_NONE, 0, D3D12_RESOURCE_STATE_GENERIC_READ);
	ThrowIfFailed(InstanceDescs->pResource->Map(0, nullptr, reinterpret_cast<void**>(&pInstanceDescs)));
}

void RaytracingAccelerationStructure::Clear()
{
	TopLevelAccelerationStructure.Clear();
	MeshRenderers.clear();
	InstanceContributionToHitGroupIndex = 0;
}

void RaytracingAccelerationStructure::AddInstance(MeshRenderer* pMeshRenderer)
{
	auto& AccelerationStructure = pMeshRenderer->pMeshFilter->Mesh->AccelerationStructure;

	Entity Entity(pMeshRenderer->Handle, pMeshRenderer->pScene);

	auto& TransformComponent = Entity.GetComponent<Transform>();

	D3D12_RAYTRACING_INSTANCE_DESC Desc = {};
	XMStoreFloat3x4(reinterpret_cast<XMFLOAT3X4*>(Desc.Transform), TransformComponent.Matrix());
	Desc.InstanceID = TopLevelAccelerationStructure.Size();
	Desc.InstanceMask = RAYTRACING_INSTANCEMASK_ALL;
	Desc.InstanceContributionToHitGroupIndex = InstanceContributionToHitGroupIndex;
	Desc.Flags = D3D12_RAYTRACING_INSTANCE_FLAG_NONE;
	Desc.AccelerationStructure = AccelerationStructure->pResource->GetGPUVirtualAddress();

	TopLevelAccelerationStructure.AddInstance(Desc);
	MeshRenderers.push_back(pMeshRenderer);

	InstanceContributionToHitGroupIndex += pMeshRenderer->pMeshFilter->Mesh->BLAS.Size() * NumHitGroups;
}

void RaytracingAccelerationStructure::Build(CommandList& CommandList)
{
	auto& RenderDevice = RenderDevice::Instance();

	PIXScopedEvent(CommandList.GetApiHandle(), 0, L"Top Level Acceleration Structure Generation");

	UINT64 ScratchSIB, ResultSIB;
	TopLevelAccelerationStructure.ComputeMemoryRequirements(RenderDevice.Device, &ScratchSIB, &ResultSIB);

	D3D12MA::ALLOCATION_DESC AllocDesc = {};
	AllocDesc.Flags = D3D12MA::ALLOCATION_FLAG_COMMITTED;
	AllocDesc.HeapType = D3D12_HEAP_TYPE_DEFAULT;

	if (!TLASScratch || TLASScratch->pResource->GetDesc().Width < ScratchSIB)
	{
		// TLAS Scratch
		TLASScratch = RenderDevice.CreateBuffer(&AllocDesc, ScratchSIB, D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS, 0, D3D12_RESOURCE_STATE_UNORDERED_ACCESS);
	}

	if (!TLASResult || TLASResult->pResource->GetDesc().Width < ResultSIB)
	{
		// TLAS Result
		TLASResult = RenderDevice.CreateBuffer(&AllocDesc, ResultSIB, D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS, 0, D3D12_RESOURCE_STATE_RAYTRACING_ACCELERATION_STRUCTURE);
	}

	// Create the description for each instance
	for (auto [i, instance] : enumerate(TopLevelAccelerationStructure))
	{
		pInstanceDescs[i] = instance;
	}

	TopLevelAccelerationStructure.Generate(CommandList, TLASScratch->pResource.Get(), TLASResult->pResource.Get(), InstanceDescs->pResource->GetGPUVirtualAddress());
}