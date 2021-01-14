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

	void UploadTextures(CommandList& CommandList);
	void UploadModels(CommandList& CommandList);
	void DisposeResources();

	void SetSelectedInstanceID(INT SelectedInstanceID);

	void RenderGui();
	bool Update(float AspectRatio);
	void CreateTopLevelAS(CommandList& CommandList);

	inline auto GetLightTable()						const { return m_LightTable; }
	inline auto GetMaterialTable()					const { return m_MaterialTable; }
	inline auto GetMeshTable()						const { return m_MeshTable; }
	inline auto GetTopLevelAccelerationStructure()	const { return m_RaytracingTopLevelAccelerationStructure.Result; }

	HLSL::Camera GetHLSLCamera() const;
	HLSL::Camera GetHLSLPreviousCamera() const;
private:
	void CreateBottomLevelAS(CommandList& CommandList);
public:
	Scene* pScene;
	BufferManager BufferManager;
	TextureManager TextureManager;
private:
	struct RTTLAS
	{
		RenderResourceHandle Scratch;
		RenderResourceHandle Result;
	};

	struct RTBLAS
	{
		RenderResourceHandle Scratch;
		RenderResourceHandle Result;
		BottomLevelAccelerationStructure BottomLevelAccelerationStructure;
	};

	RenderDevice*			pRenderDevice;

	RenderResourceHandle	m_LightTable;
	RenderResourceHandle	m_MaterialTable;
	RenderResourceHandle	m_InstanceDescsBuffer;
	RenderResourceHandle	m_MeshTable;

	std::vector<RTBLAS>		m_RaytracingBottomLevelAccelerationStructures;
	RTTLAS					m_RaytracingTopLevelAccelerationStructure;

	TopLevelAccelerationStructure m_TopLevelAccelerationStructure;

	INT						m_SelectedInstanceID	= -1;
};