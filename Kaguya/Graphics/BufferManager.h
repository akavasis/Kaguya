#pragma once
#include <d3d12.h>
#include <vector>
#include <unordered_map>

#include "RenderDevice.h"
#include "RenderContext.h"
#include "Scene/Scene.h"

class BufferManager
{
public:
	BufferManager(RenderDevice* pRenderDevice);

	void Stage(Scene& Scene, CommandList& CommandList);
	void DisposeResources();
private:
	struct StagingBuffer
	{
		Microsoft::WRL::ComPtr<ID3D12Resource> Buffer;
		D3D12MA::Allocation* pAllocation;
	};

	StagingBuffer CreateStagingBuffer(const D3D12_RESOURCE_DESC& Desc, const void* pData);

	void LoadMesh(Mesh& Mesh);
	void StageVertexResource(Mesh& Mesh);
	void StageIndexResource(Mesh& Mesh);
private:
	RenderDevice*											pRenderDevice;
	D3D12MA::Allocator*										pAllocator;

	std::unordered_map<RenderResourceHandle, StagingBuffer>	m_UnstagedBuffers;
};