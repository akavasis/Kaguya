#pragma once
#include <d3d12.h>
#include <vector>
#include <unordered_set>
#include <unordered_map>

#include "RenderDevice.h"
#include "RenderContext.h"
#include "Scene/Scene.h"

class BufferManager
{
public:
	BufferManager(RenderDevice* pRenderDevice);

	void Stage(Scene& Scene, RenderContext& RenderContext);
	void DisposeResources();
private:
	struct StagingBuffer
	{
		Buffer Buffer; // gpu upload buffer
	};

	StagingBuffer CreateStagingBuffer(D3D12_RESOURCE_DESC Desc, const void* pData);

	void LoadModel(Model& Model);
	void StageVertexResource(Model& Model);
	void StageIndexResource(Model& Model);
	void StageBuffer(RenderResourceHandle Handle, StagingBuffer& StagingBuffer, RenderContext& RenderContext);

	RenderDevice*											pRenderDevice;

	std::unordered_map<RenderResourceHandle, StagingBuffer>	m_UnstagedBuffers;
};