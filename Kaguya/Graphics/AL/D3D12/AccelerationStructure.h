#pragma once
#include "Math/Transform.h"

class Device;
class Buffer;
class CommandContext;

struct RaytracingGeometryDesc
{
	Buffer* pVertexBuffer;
	UINT NumVertices;
	UINT VertexStride;
	UINT64 VertexOffset;

	Buffer* pIndexBuffer;
	UINT NumIndices;
	UINT IndexStride;
	UINT64 IndexOffset;

	Buffer* pTransformBuffer;
	bool IsOpaque;
};

class RaytracingAccelerationStructure
{
public:
	RaytracingAccelerationStructure(const Device* pDevice);

	void SetAccelerationStructure(Buffer* pResult, Buffer* pScratch, Buffer* pSource);

	inline auto GetResultBuffer() const { return pResult; }
protected:
	const Device* pDevice;

	Buffer* pResult;
	Buffer* pScratch;
	Buffer* pSource;
	D3D12_BUILD_RAYTRACING_ACCELERATION_STRUCTURE_DESC m_ASDesc;
	D3D12_RAYTRACING_ACCELERATION_STRUCTURE_BUILD_FLAGS m_ASBuildFlags;
};

class BottomLevelAccelerationStructure : public RaytracingAccelerationStructure
{
public:
	struct MemoryRequirements
	{
		UINT64 ResultDataMaxSizeInBytes;
		UINT64 ScratchDataSizeInBytes;
		UINT64 UpdateScratchDataSizeInBytes;
	};

	BottomLevelAccelerationStructure(const Device* pDevice);

	void AddGeometry(const RaytracingGeometryDesc& Desc);
	MemoryRequirements GetAccelerationStructureMemoryRequirements(bool AllowUpdate);
	void Generate(bool UpdateOnly, CommandContext* pCommandContext);
private:
	std::vector<D3D12_RAYTRACING_GEOMETRY_DESC> m_RTGeometryDescs;
	MemoryRequirements m_MemoryRequirements;
};

class TopLevelAccelerationStructure : public RaytracingAccelerationStructure
{
public:
	struct MemoryRequirements
	{
		UINT64 ResultDataMaxSizeInBytes;
		UINT64 ScratchDataSizeInBytes;
		UINT64 UpdateScratchDataSizeInBytes;
		UINT64 InstanceDataSizeInBytes;
	};

	TopLevelAccelerationStructure(const Device* pDevice);

	void SetInstanceBuffer(Buffer* pInstance);

	void AddInstance(Buffer* pBLAS, Transform Transform, UINT InstanceID, UINT HitGroupIndex);
	MemoryRequirements GetAccelerationStructureMemoryRequirements(bool AllowUpdate);
	void Generate(bool UpdateOnly, CommandContext* pCommandContext);
private:
	std::vector<D3D12_RAYTRACING_INSTANCE_DESC> m_RTInstanceDescs;
	MemoryRequirements m_MemoryRequirements;
	Buffer* m_pInstance;
};