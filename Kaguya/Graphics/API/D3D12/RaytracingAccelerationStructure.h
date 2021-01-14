#pragma once
#include <d3d12.h>
#include <vector>

class BottomLevelAccelerationStructure
{
public:
	BottomLevelAccelerationStructure();

	void AddGeometry(const D3D12_RAYTRACING_GEOMETRY_DESC& Desc);
	void ComputeMemoryRequirements(ID3D12Device5* pDevice, UINT64* pScratchSizeInBytes, UINT64* pResultSizeInBytes);
	void Generate(ID3D12GraphicsCommandList6* pCommandList, ID3D12Resource* pScratch, ID3D12Resource* pResult);
private:
	std::vector<D3D12_RAYTRACING_GEOMETRY_DESC>	RaytracingGeometryDescs;
	UINT64										ScratchSizeInBytes;
	UINT64										ResultSizeInBytes;
};

// https://developer.nvidia.com/blog/rtx-best-practices/
// We should rebuild the TLAS rather than update, It’s just easier to manage in most circumstances, and the cost savings to refit likely aren’t worth sacrificing quality of TLAS.
class TopLevelAccelerationStructure
{
public:
	TopLevelAccelerationStructure();

	void SetNumInstances(UINT NumInstances);
	void ComputeMemoryRequirements(ID3D12Device5* pDevice, UINT64* pScratchSizeInBytes, UINT64* pResultSizeInBytes);
	void Generate(ID3D12GraphicsCommandList6* pCommandList, ID3D12Resource* pScratch, ID3D12Resource* pResult, ID3D12Resource* pInstanceDescs);
private:
	UINT	NumInstances;
	UINT64	ScratchSizeInBytes;
	UINT64	ResultSizeInBytes;
};