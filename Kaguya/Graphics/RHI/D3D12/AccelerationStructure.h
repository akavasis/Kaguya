#pragma once

class BottomLevelAccelerationStructure
{
public:
	BottomLevelAccelerationStructure();

	auto Size() const
	{
		return RaytracingGeometryDescs.size();
	}

	void Clear()
	{
		RaytracingGeometryDescs.clear();
	}

	void AddGeometry(const D3D12_RAYTRACING_GEOMETRY_DESC& Desc);
	void ComputeMemoryRequirements(ID3D12Device5* pDevice, UINT64* pScratchSizeInBytes, UINT64* pResultSizeInBytes);
	void Generate(ID3D12GraphicsCommandList6* pCommandList, ID3D12Resource* pScratch, ID3D12Resource* pResult);
private:
	std::vector<D3D12_RAYTRACING_GEOMETRY_DESC>	RaytracingGeometryDescs;
	UINT64 ScratchSizeInBytes;
	UINT64 ResultSizeInBytes;
};

// https://developer.nvidia.com/blog/rtx-best-practices/
// We should rebuild the TLAS rather than update, It�s just easier to manage in most circumstances, and the cost savings to refit likely aren�t worth sacrificing quality of TLAS.
class TopLevelAccelerationStructure
{
public:
	TopLevelAccelerationStructure();

	auto Size() const
	{
		return RaytracingInstanceDescs.size();
	}

	bool Empty() const
	{
		return RaytracingInstanceDescs.empty();
	}

	void Clear()
	{
		RaytracingInstanceDescs.clear();
	}

	auto& operator[](size_t i)
	{
		return RaytracingInstanceDescs[i];
	}

	auto& operator[](size_t i) const
	{
		return RaytracingInstanceDescs[i];
	}

	auto begin()
	{
		return RaytracingInstanceDescs.begin();
	}

	auto end()
	{
		return RaytracingInstanceDescs.end();
	}

	void AddInstance(const D3D12_RAYTRACING_INSTANCE_DESC& Desc);
	void ComputeMemoryRequirements(ID3D12Device5* pDevice, UINT64* pScratchSizeInBytes, UINT64* pResultSizeInBytes);
	void Generate(ID3D12GraphicsCommandList6* pCommandList, ID3D12Resource* pScratch, ID3D12Resource* pResult, D3D12_GPU_VIRTUAL_ADDRESS InstanceDescs);
private:
	std::vector<D3D12_RAYTRACING_INSTANCE_DESC> RaytracingInstanceDescs;
	UINT64 ScratchSizeInBytes;
	UINT64 ResultSizeInBytes;
};