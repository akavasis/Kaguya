#include "pch.h"
#include "RTScene.h"

#include "RendererRegistry.h"

using namespace DirectX;

void RaytracingAccelerationStructure::Create(RenderDevice* pRenderDevice)
{
	this->pRenderDevice = pRenderDevice;

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
}

void RaytracingAccelerationStructure::Clear()
{
	TopLevelAccelerationStructure.Clear();
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

	InstanceContributionToHitGroupIndex += pMeshRenderer->pMeshFilter->pMesh->BLAS.Size();
}

//void RaytracingAccelerationStructure::AddRTGeometry(const Mesh& Mesh)
//{
//	//// Update mesh's BLAS Index
//	//Mesh.BottomLevelAccelerationStructureIndex = m_RaytracingBottomLevelAccelerationStructures.size();
//
//	//D3D12_RAYTRACING_GEOMETRY_DESC Desc = {};
//	//Desc.Type = D3D12_RAYTRACING_GEOMETRY_TYPE_TRIANGLES;
//	//Desc.Flags = D3D12_RAYTRACING_GEOMETRY_FLAG_OPAQUE;
//	//Desc.Triangles.Transform3x4 = NULL;
//	//Desc.Triangles.IndexFormat = DXGI_FORMAT_R32_UINT;
//	//Desc.Triangles.VertexFormat = DXGI_FORMAT_R32G32B32_FLOAT; // Position attribute of the vertex
//	//Desc.Triangles.IndexCount = Mesh.IndexCount;
//	//Desc.Triangles.VertexCount = Mesh.VertexCount;
//	//Desc.Triangles.IndexBuffer = Mesh.IndexResource->GetGPUVirtualAddress();
//	//Desc.Triangles.VertexBuffer.StartAddress = Mesh.VertexResource->GetGPUVirtualAddress();
//	//Desc.Triangles.VertexBuffer.StrideInBytes = sizeof(Vertex);
//
//	//rtblas.BottomLevelAccelerationStructure.AddGeometry(Desc);
//
//	//m_RaytracingBottomLevelAccelerationStructures.push_back(rtblas);
//}

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

	UINT64 ScratchSizeInBytes, ResultSizeInBytes;
	TopLevelAccelerationStructure.ComputeMemoryRequirements(pRenderDevice->Device, &ScratchSizeInBytes, &ResultSizeInBytes);

	//if (!TLASScratch || TLASScratch->GetDesc().Width < ScratchSizeInBytes)
	//{
	//	TLASScratch.Reset();

	//	// TLAS Scratch
	//	auto HeapProperties = CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_DEFAULT);
	//	auto ResourceDesc = CD3DX12_RESOURCE_DESC::Buffer(ScratchSizeInBytes, D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS);
	//	Context.pRenderDevice->Device->CreateCommittedResource(&HeapProperties, D3D12_HEAP_FLAG_NONE,
	//		&ResourceDesc, D3D12_RESOURCE_STATE_UNORDERED_ACCESS, nullptr,
	//		IID_PPV_ARGS(TLASScratch.ReleaseAndGetAddressOf()));
	//}

	//if (!TLASResult || TLASResult->GetDesc().Width < ResultSizeInBytes)
	//{
	//	TLASResult.Reset();

	//	// TLAS Result
	//	auto HeapProperties = CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_DEFAULT);
	//	auto ResourceDesc = CD3DX12_RESOURCE_DESC::Buffer(ResultSizeInBytes, D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS);
	//	Context.pRenderDevice->Device->CreateCommittedResource(&HeapProperties, D3D12_HEAP_FLAG_NONE,
	//		&ResourceDesc, D3D12_RESOURCE_STATE_RAYTRACING_ACCELERATION_STRUCTURE, nullptr,
	//		IID_PPV_ARGS(TLASResult.ReleaseAndGetAddressOf()));
	//}

	//TopLevelAccelerationStructure.Generate(CommandList, TLASScratch.Get(), TLASResult.Get(), pInstanceDescsBuffer);
}