#include "pch.h"
#include "RTScene.h"

#include "RendererRegistry.h"

using namespace DirectX;

RaytracingAccelerationStructure::~RaytracingAccelerationStructure()
{
	if (InstanceDescs)
	{
		InstanceDescs->pResource->Unmap(0, nullptr);
	}
}

void RaytracingAccelerationStructure::Create(RenderDevice* pRenderDevice, UINT NumHitGroups)
{
	this->pRenderDevice = pRenderDevice;
	this->NumHitGroups = NumHitGroups;

	//RS = pRenderDevice->CreateRootSignature([](RootSignatureBuilder& Builder)
	//{
	//	Builder.AddRootConstantsParameter(RootConstants<void>(0, 0, 1));

	//	Builder.AddRootSRVParameter(RootSRV(0, 0));
	//	Builder.AddRootUAVParameter(RootUAV(0, 0));
	//}, false);

	//PSO = pRenderDevice->CreateComputePipelineState([=](ComputePipelineStateBuilder& Builder)
	//{
	//	Builder.pRootSignature = &RS;
	//	Builder.pCS = &Shaders::CS::InstanceGeneration;
	//});

	D3D12MA::ALLOCATION_DESC Desc = {};
	Desc.HeapType = D3D12_HEAP_TYPE_UPLOAD;
	InstanceDescs = pRenderDevice->CreateBuffer(&Desc, sizeof(D3D12_RAYTRACING_INSTANCE_DESC) * Scene::MAX_INSTANCE_SUPPORTED, D3D12_RESOURCE_FLAG_NONE, 0, D3D12_RESOURCE_STATE_GENERIC_READ);
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
	auto& AccelerationStructure = pMeshRenderer->pMeshFilter->pMesh->AccelerationStructure;

	Entity Entity(pMeshRenderer->Handle, pMeshRenderer->pScene);

	auto& TransformComponent = Entity.GetComponent<Transform>();

	D3D12_RAYTRACING_INSTANCE_DESC Desc = {};
	XMStoreFloat3x4(reinterpret_cast<XMFLOAT3X4*>(Desc.Transform), TransformComponent.Matrix());
	Desc.InstanceID = 0;
	Desc.InstanceMask = RAYTRACING_INSTANCEMASK_ALL;
	Desc.InstanceContributionToHitGroupIndex = InstanceContributionToHitGroupIndex;
	Desc.Flags = D3D12_RAYTRACING_INSTANCE_FLAG_NONE;
	Desc.AccelerationStructure = AccelerationStructure->pResource->GetGPUVirtualAddress();

	TopLevelAccelerationStructure.AddInstance(Desc);
	MeshRenderers.push_back(pMeshRenderer);

	InstanceContributionToHitGroupIndex += pMeshRenderer->pMeshFilter->pMesh->BLAS.Size() * NumHitGroups;
}

void RaytracingAccelerationStructure::Build(CommandList& CommandList)
{
	PIXScopedEvent(CommandList.GetApiHandle(), 0, L"Top Level Acceleration Structure Generation");

	/*CommandList->SetPipelineState(PSO);
	CommandList->SetComputeRootSignature(RS);

	auto pMeshBuffer = pRenderDevice->GetBuffer(m_MeshTable)->GetApiHandle();
	auto pInstanceDescsBuffer = pRenderDevice->GetBuffer(m_InstanceDescsBuffer)->GetApiHandle();
	CommandList->SetComputeRoot32BitConstant(0, NumInstances, 0);
	CommandList->SetComputeRootShaderResourceView(1, pMeshBuffer->GetGPUVirtualAddress());
	CommandList->SetComputeRootUnorderedAccessView(2, pInstanceDescsBuffer->GetGPUVirtualAddress());*/

	//CommandList.Dispatch1D<64>(NumInstances);
	//CommandList.UAVBarrier(pInstanceDescsBuffer);
	//CommandList.FlushResourceBarriers();

	UINT64 ScratchSIB, ResultSIB;
	TopLevelAccelerationStructure.ComputeMemoryRequirements(pRenderDevice->Device, &ScratchSIB, &ResultSIB);

	D3D12MA::ALLOCATION_DESC AllocDesc = {};
	AllocDesc.Flags = D3D12MA::ALLOCATION_FLAG_COMMITTED;
	AllocDesc.HeapType = D3D12_HEAP_TYPE_DEFAULT;

	if (!TLASScratch || TLASScratch->pResource->GetDesc().Width < ScratchSIB)
	{
		// TLAS Scratch
		TLASScratch = pRenderDevice->CreateBuffer(&AllocDesc, ScratchSIB, D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS, 0, D3D12_RESOURCE_STATE_UNORDERED_ACCESS);
	}

	if (!TLASResult || TLASResult->pResource->GetDesc().Width < ResultSIB)
	{
		// TLAS Result
		TLASResult = pRenderDevice->CreateBuffer(&AllocDesc, ResultSIB, D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS, 0, D3D12_RESOURCE_STATE_RAYTRACING_ACCELERATION_STRUCTURE);
	}

	// Create the description for each instance
	for (auto [i, instance] : enumerate(TopLevelAccelerationStructure))
	{
		pInstanceDescs[i] = instance;
	}

	TopLevelAccelerationStructure.Generate(CommandList, TLASScratch->pResource.Get(), TLASResult->pResource.Get(), InstanceDescs->pResource->GetGPUVirtualAddress());
}