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
		LightTable,
		MaterialTable,
		VertexBuffer,
		IndexBuffer,
		GeometryInfoTable,
		NumResources
	};

	GpuScene(RenderDevice* pRenderDevice);

	void UploadLights();
	void UploadMaterials();
	void UploadModels();
	void UploadModelInstances();
	void Commit(RenderContext& RenderContext);

	void Update(float AspectRatio);

	inline auto GetLightTableHandle()			const		{ return ResourceTables[LightTable]; }
	inline auto GetMaterialTableHandle()		const		{ return ResourceTables[MaterialTable]; }
	inline auto GetVertexBufferHandle()			const		{ return ResourceTables[VertexBuffer]; }
	inline auto GetIndexBufferHandle()			const		{ return ResourceTables[IndexBuffer]; }
	inline auto GetGeometryInfoTableHandle()	const		{ return ResourceTables[GeometryInfoTable]; }
	inline auto GetRTTLASResourceHandle()		const		{ return m_RaytracingTopLevelAccelerationStructure.Handles.Result; }

	inline auto GetVertexBufferView()			const		{ return &VertexBufferView; }
	inline auto GetIndexBufferView()			const		{ return &IndexBufferView; }

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

	D3D12_VERTEX_BUFFER_VIEW VertexBufferView;
	D3D12_INDEX_BUFFER_VIEW IndexBufferView;

	std::vector<RTBLAS> m_RaytracingBottomLevelAccelerationStructures;
	RTTLAS m_RaytracingTopLevelAccelerationStructure;
};