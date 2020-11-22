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
	GpuScene(RenderDevice* pRenderDevice);

	void UploadTextures(RenderContext& RenderContext);
	void UploadModels(RenderContext& RenderContext);
	void DisposeResources();

	void UploadLights();
	void UploadMaterials();
	void UploadMeshes();

	void RenderGui();
	bool Update(float AspectRatio);
	void CreateTopLevelAS(RenderContext& RenderContext);

	inline auto GetLightTableHandle()			const { return m_LightTable; }
	inline auto GetMaterialTableHandle()		const { return m_MaterialTable; }
	inline auto GetMeshTable()					const { return m_MeshTable; }
	inline auto GetVertexBufferHandle()			const { return m_VertexBuffer; }
	inline auto GetIndexBufferHandle()			const { return m_IndexBuffer; }
	inline auto GetRTTLASResourceHandle()		const { return m_TopLevelAccelerationStructure.Result; }

	HLSL::Camera GetHLSLCamera() const;
	HLSL::Camera GetHLSLPreviousCamera() const;

	Scene* pScene;
	GpuTextureAllocator GpuTextureAllocator;
private:
	void CreateBottomLevelAS(RenderContext& RenderContext);

	struct RTBLAS
	{
		RaytracingAccelerationStructureHandles Handles;
		BottomLevelAccelerationStructure BLAS;
	};

	RenderDevice*			SV_pRenderDevice;

	RenderResourceHandle	m_LightTable;
	RenderResourceHandle	m_MaterialTable;
	RenderResourceHandle	m_MeshTable;

	RenderResourceHandle	m_VertexBuffer, m_UploadVertexBuffer;
	VariableSizedAllocator	m_VertexBufferAllocator;

	RenderResourceHandle	m_IndexBuffer, m_UploadIndexBuffer;
	VariableSizedAllocator	m_IndexBufferAllocator;

	std::vector<RTBLAS>		m_RaytracingBottomLevelAccelerationStructures;
	RaytracingAccelerationStructureHandles m_TopLevelAccelerationStructure;
};