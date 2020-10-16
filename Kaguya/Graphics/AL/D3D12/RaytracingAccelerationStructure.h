#pragma once
#include <DirectXMath.h>

class Device;
class DeviceBuffer;
class CommandContext;

struct RaytracingAccelerationStructureBuffers
{
	DeviceBuffer* pScratch;
	DeviceBuffer* pResult;
	DeviceBuffer* pInstanceDescs;
};

struct RaytracingGeometryDesc
{
	DeviceBuffer* pVertexBuffer;
	UINT NumVertices;
	UINT VertexStride;
	UINT64 VertexOffset;

	DeviceBuffer* pIndexBuffer;
	UINT NumIndices;
	UINT IndexStride;
	UINT64 IndexOffset;

	DeviceBuffer* pTransformBuffer;
	bool IsOpaque;
};

class BottomLevelAccelerationStructure
{
public:
	BottomLevelAccelerationStructure();

	void AddGeometry(const RaytracingGeometryDesc& Desc);
	void ComputeMemoryRequirements(const Device* pDevice, bool AllowUpdate, UINT64* pScratchSizeInBytes, UINT64* pResultSizeInBytes);
	void Generate(CommandContext* pCommandContext, DeviceBuffer* pScratch, DeviceBuffer* pResult, bool UpdateOnly = false, DeviceBuffer* pSource = nullptr);
private:
	std::vector<D3D12_RAYTRACING_GEOMETRY_DESC> RaytracingGeometryDescs;
	UINT64 ScratchSizeInBytes;
	UINT64 ResultSizeInBytes;
	D3D12_RAYTRACING_ACCELERATION_STRUCTURE_BUILD_FLAGS BuildFlags;
};

struct RaytracingInstanceDesc
{
	D3D12_GPU_VIRTUAL_ADDRESS AccelerationStructure;
	DirectX::XMMATRIX Transform;
	UINT InstanceID;
	UINT InstanceMask;

	UINT HitGroupIndex;
};

class TopLevelAccelerationStructure
{
public:
	TopLevelAccelerationStructure();

	void AddInstance(const RaytracingInstanceDesc& Desc);
	void ComputeMemoryRequirements(const Device* pDevice, bool AllowUpdate, UINT64* pScratchSizeInBytes, UINT64* pResultSizeInBytes, UINT64* pInstanceDescsSizeInBytes);
	void Generate(CommandContext* pCommandContext, DeviceBuffer* pScratch, DeviceBuffer* pResult, DeviceBuffer* pInstanceDescs, bool UpdateOnly = false, DeviceBuffer* pSource = nullptr);
private:
	std::vector<D3D12_RAYTRACING_INSTANCE_DESC> RaytracingInstanceDescs;
	UINT64 ScratchSizeInBytes;
	UINT64 ResultSizeInBytes;
	UINT64 InstanceDescsSizeInBytes;
	D3D12_RAYTRACING_ACCELERATION_STRUCTURE_BUILD_FLAGS BuildFlags;
};