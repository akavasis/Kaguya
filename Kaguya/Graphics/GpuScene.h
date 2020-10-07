#pragma once
#include <vector>
#include "Scene/Scene.h"
#include "GpuTextureAllocator.h"
#include "RenderDevice.h"
#include "RenderContext.h"

class GpuScene
{
public:
	enum EResource
	{
		MaterialTable,
		VertexBuffer,
		IndexBuffer,
		GeometryInfoTable,
		NumResources
	};

	GpuScene(RenderDevice* pRenderDevice);

	void UploadMaterials();
	void UploadModels();
	void UploadModelInstances();
	void Commit(RenderContext& RenderContext);

	void Update();

	inline auto GetMaterialTableHandle() const { return ResourceTables[MaterialTable]; }
	inline auto GetVertexBufferHandle() const { return ResourceTables[VertexBuffer]; }
	inline auto GetIndexBufferHandle() const { return ResourceTables[IndexBuffer]; }
	inline auto GetGeometryInfoTableHandle() const { return ResourceTables[GeometryInfoTable]; }
	inline auto GetRTTLASResourceHandle() const { return m_RaytracingTopLevelAccelerationStructure.Handles.Result; }

	Scene* pScene;
	GpuTextureAllocator GpuTextureAllocator;
private:
	size_t Upload(EResource Type, const void* pData, size_t ByteSize, Buffer* pUploadBuffer);
	void CreateBottomLevelAS(RenderContext& RenderContext);
	void CreateTopLevelAS(RenderContext& RenderContext);

	struct RTBLAS
	{
		RaytracingAccelerationStructureHandles Handles;
		BottomLevelAccelerationStructure BLAS;
	};

	struct RTTLAS
	{
		RaytracingAccelerationStructureHandles Handles;
		TopLevelAccelerationStructure TLAS;
	};

	RenderDevice* pRenderDevice;

	RenderResourceHandle UploadResourceTables[NumResources];
	RenderResourceHandle ResourceTables[NumResources];
	VariableSizedAllocator Allocators[NumResources];

	std::vector<RTBLAS> m_RaytracingBottomLevelAccelerationStructures;
	RTTLAS m_RaytracingTopLevelAccelerationStructure;
};