#pragma once
#include <d3d12.h>
#include <DirectXMath.h>

class Device;
class Buffer;
class CommandContext;

struct RaytracingGeometryDesc
{
	Buffer* pVertexBuffer;
	UINT	NumVertices;
	UINT	VertexStride;
	UINT64	VertexOffset;

	Buffer* pIndexBuffer;
	UINT	NumIndices;
	UINT	IndexStride;
	UINT64	IndexOffset;

	bool	IsOpaque;
};

class BottomLevelAccelerationStructure
{
public:
	BottomLevelAccelerationStructure();

	void AddGeometry(const RaytracingGeometryDesc& Desc);
	void ComputeMemoryRequirements(ID3D12Device5* pDevice, UINT64* pScratchSizeInBytes, UINT64* pResultSizeInBytes);
	void Generate(CommandContext* pCommandContext, Buffer* pScratch, Buffer* pResult);
private:
	std::vector<D3D12_RAYTRACING_GEOMETRY_DESC>			RaytracingGeometryDescs;
	UINT64												ScratchSizeInBytes;
	UINT64												ResultSizeInBytes;
	D3D12_RAYTRACING_ACCELERATION_STRUCTURE_BUILD_FLAGS BuildFlags;
};

struct RaytracingInstanceDesc
{
	D3D12_GPU_VIRTUAL_ADDRESS	AccelerationStructure;
	DirectX::XMMATRIX			Transform;
	UINT						InstanceID;
	UINT						InstanceMask;

	UINT						InstanceContributionToHitGroupIndex;
};

// https://developer.nvidia.com/blog/rtx-best-practices/
// We should rebuild the TLAS rather than update, It’s just easier to manage in most circumstances, and the cost savings to refit likely aren’t worth sacrificing quality of TLAS.
class TopLevelAccelerationStructure
{
public:
	TopLevelAccelerationStructure();

	const D3D12_RAYTRACING_INSTANCE_DESC& operator[](size_t Index)
	{
		return RaytracingInstanceDescs[Index];
	}

	void AddInstance(const RaytracingInstanceDesc& Desc);
	void ComputeMemoryRequirements(ID3D12Device5* pDevice, UINT64* pScratchSizeInBytes, UINT64* pResultSizeInBytes, UINT64* pInstanceDescsSizeInBytes);
	void Generate(CommandContext* pCommandContext, Buffer* pScratch, Buffer* pResult, Buffer* pInstanceDescs);
private:
	std::vector<D3D12_RAYTRACING_INSTANCE_DESC>			RaytracingInstanceDescs;
	UINT64												ScratchSizeInBytes;
	UINT64												ResultSizeInBytes;
	UINT64												InstanceDescsSizeInBytes;
	D3D12_RAYTRACING_ACCELERATION_STRUCTURE_BUILD_FLAGS BuildFlags;
};