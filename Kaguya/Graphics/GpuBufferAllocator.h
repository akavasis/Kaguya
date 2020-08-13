#pragma once
#include "RenderDevice.h"
#include "Core/Allocator/VariableSizedAllocator.h"
#include "Scene/Scene.h"
#include "AL/D3D12/RaytracingAccelerationStructure.h"

struct RaytracingAccelerationStructureHandles
{
	RenderResourceHandle Scratch;
	RenderResourceHandle Result;
	RenderResourceHandle InstanceDescs;
};

class GpuBufferAllocator
{
public:
	GpuBufferAllocator(RenderDevice* pRenderDevice, size_t VertexBufferByteSize, size_t IndexBufferByteSize, size_t ConstantBufferByteSize);

	inline auto GetVertexBuffer() const { return m_Buffers[VertexBuffer].pBuffer; }
	inline auto GetIndexBuffer() const { return m_Buffers[IndexBuffer].pBuffer; }
	inline auto GetConstantBuffer() const { return m_Buffers[ConstantBuffer].pUploadBuffer; }

	inline auto GetVertexBufferHandle() const { return m_Buffers[VertexBuffer].BufferHandle; }
	inline auto GetIndexBufferHandle() const { return m_Buffers[IndexBuffer].BufferHandle; }
	inline auto GetRTTLASResourceHandle() const { return m_RaytracingTopLevelAccelerationStructure.Handles.Result; }
	inline auto GetRayGenerationShaderTableHandle() const { return RayGenerationShaderTable; }
	inline auto GetMissShaderTableHandle() const { return MissShaderTable; }
	inline auto GetHitGroupShaderTableHandle() const { return HitGroupShaderTable; }

	void Stage(Scene& Scene, CommandContext* pCommandContext);
	void Update(Scene& Scene);
	void Bind(CommandContext* pCommandContext) const;
private:
	size_t StageVertex(const void* pData, size_t ByteSize, CommandContext* pCommandContext);
	size_t StageIndex(const void* pData, size_t ByteSize, CommandContext* pCommandContext);
	void CreateBottomLevelAS(Scene& Scene, CommandContext* pCommandContext);
	void CreateTopLevelAS(Scene& Scene, CommandContext* pCommandContext);
	void CreateShaderTableBuffers(Scene& Scene);

	enum InternalBufferTypes
	{
		VertexBuffer,
		IndexBuffer,
		ConstantBuffer,
		NumInternalBufferTypes
	};

	struct InternalBufferStructure
	{
		VariableSizedAllocator Allocator;
		RenderResourceHandle BufferHandle;
		Buffer* pBuffer;
		RenderResourceHandle UploadBufferHandle;
		Buffer* pUploadBuffer;

		InternalBufferStructure(size_t BufferByteSize)
			: Allocator(BufferByteSize),
			BufferHandle(),
			pBuffer(nullptr),
			UploadBufferHandle(),
			pUploadBuffer(nullptr)
		{
		}

		auto Allocate(size_t ByteSize)
		{
			return Allocator.Allocate(ByteSize);
		}
	};

	struct RTBLAS
	{
		RaytracingAccelerationStructureHandles Handles;
		RaytracingAccelerationStructureBuffers Buffers;
		BottomLevelAccelerationStructure BLAS;
	};

	struct RTTLAS
	{
		RaytracingAccelerationStructureHandles Handles;
		RaytracingAccelerationStructureBuffers Buffers;
		TopLevelAccelerationStructure TLAS;
	};

	RenderDevice* pRenderDevice;

	UINT m_BufferStrides[NumInternalBufferTypes];
	InternalBufferStructure m_Buffers[NumInternalBufferTypes];
	UINT m_NumVertices;
	UINT m_NumIndices;

	std::vector<RTBLAS> m_RaytracingBottomLevelAccelerationStructures;
	RTTLAS m_RaytracingTopLevelAccelerationStructure;

	RenderResourceHandle RayGenerationShaderTable;
	RenderResourceHandle MissShaderTable;
	RenderResourceHandle HitGroupShaderTable;
};