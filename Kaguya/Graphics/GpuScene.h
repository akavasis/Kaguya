#pragma once
#include <vector>

#include "Scene/Scene.h"
#include "BufferManager.h"
#include "TextureManager.h"
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

	inline auto GetLightTable()						const { return m_LightTable; }
	inline auto GetMaterialTable()					const { return m_MaterialTable; }
	inline auto GetMeshTable()						const { return m_MeshTable; }
	inline auto GetTopLevelAccelerationStructure()	const { return m_RaytracingTopLevelAccelerationStructure.Result; }

	HLSL::Camera GetHLSLCamera() const;
	HLSL::Camera GetHLSLPreviousCamera() const;

private:
	void CreateBottomLevelAS(RenderContext& RenderContext);

public:
	Scene* pScene;
	BufferManager BufferManager;
	TextureManager TextureManager;

private:
	struct RTTLAS
	{
		RenderResourceHandle Scratch;
		RenderResourceHandle Result;
		RenderResourceHandle InstanceDescs;
	};

	struct RTBLAS
	{
		RenderResourceHandle Scratch;
		RenderResourceHandle Result;
		BottomLevelAccelerationStructure BLAS;
	};

	RenderDevice*			pRenderDevice;

	RenderResourceHandle	m_LightTable;
	RenderResourceHandle	m_MaterialTable;
	RenderResourceHandle	m_MeshTable;
	RenderResourceHandle	m_PickingResult;
	RenderResourceHandle	m_PickingResultReadback;

	std::vector<RTBLAS>		m_RaytracingBottomLevelAccelerationStructures;
	RTTLAS					m_RaytracingTopLevelAccelerationStructure;
};