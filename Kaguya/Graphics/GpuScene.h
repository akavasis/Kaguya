#pragma once
#include <vector>
#include <Core/Allocator/VariableSizedAllocator.h>

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
		MeshTable,
		NumResources
	};

	GpuScene(RenderDevice* pRenderDevice);

	void UploadLights(RenderContext& RenderContext);
	void UploadMaterials(RenderContext& RenderContext);
	void UploadModels(RenderContext& RenderContext);
	void UploadModelInstances(RenderContext& RenderContext);
	void DisposeResources();

	void RenderGui();
	bool Update(float AspectRatio, RenderContext& RenderContext);

	inline auto GetLightTableHandle()			const { return m_ResourceTables[LightTable]; }
	inline auto GetMaterialTableHandle()		const { return m_ResourceTables[MaterialTable]; }
	inline auto GetVertexBufferHandle()			const { return m_ResourceTables[VertexBuffer]; }
	inline auto GetIndexBufferHandle()			const { return m_ResourceTables[IndexBuffer]; }
	inline auto GetGeometryInfoTableHandle()	const { return m_ResourceTables[MeshTable]; }
	inline auto GetRTTLASResourceHandle()		const { return m_RaytracingTopLevelAccelerationStructure.Result; }

	HLSL::Camera GetHLSLCamera() const;
	HLSL::Camera GetHLSLPreviousCamera() const;

	Scene* pScene;
	GpuTextureAllocator GpuTextureAllocator;
private:
	size_t Upload(EResource Type, const void* pData, size_t ByteSize, DeviceBuffer* pUploadBuffer);
	void CreateBottomLevelAS(RenderContext& RenderContext);
	void CreateTopLevelAS(RenderContext& RenderContext);

	struct RTBLAS
	{
		RaytracingAccelerationStructureHandles Handles;
		BottomLevelAccelerationStructure BLAS;
	};

	RenderDevice*			SV_pRenderDevice;

	RenderResourceHandle	m_UploadResourceTables[NumResources];
	RenderResourceHandle	m_ResourceTables[NumResources];
	VariableSizedAllocator	m_Allocators[NumResources];

	std::vector<RTBLAS>		m_RaytracingBottomLevelAccelerationStructures;
	RaytracingAccelerationStructureHandles m_RaytracingTopLevelAccelerationStructure;
};