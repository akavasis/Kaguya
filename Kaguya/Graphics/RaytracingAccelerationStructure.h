#pragma once
#include "RenderDevice.h"
#include "ResourceManager.h"
#include "Scene/Scene.h"
#include "Scene/Entity.h"

#define RAYTRACING_INSTANCEMASK_ALL 	(0xff)
#define RAYTRACING_INSTANCEMASK_OPAQUE 	(1 << 0)
#define RAYTRACING_INSTANCEMASK_LIGHT	(1 << 1)

class RaytracingAccelerationStructure
{
public:
	void Create(RenderDevice* pRenderDevice,
		UINT NumHitGroups);

	operator auto() const
	{
		return TLASResult->pResource->GetGPUVirtualAddress();
	}

	auto Size() const
	{
		return TopLevelAccelerationStructure.Size();
	}

	bool Empty() const
	{
		return TopLevelAccelerationStructure.Empty();
	}

	void Clear();

	void AddInstance(MeshRenderer* pMeshRenderer);

	void Build(CommandList& CommandList);
private:
	RenderDevice* pRenderDevice = nullptr;

	UINT NumHitGroups = 0;

	RootSignature RS;
	PipelineState PSO;

	TopLevelAccelerationStructure TopLevelAccelerationStructure;
	std::vector<MeshRenderer*> MeshRenderers;
	UINT InstanceContributionToHitGroupIndex = 0;

	std::shared_ptr<Resource> TLASScratch, TLASResult;
	std::shared_ptr<Resource> InstanceDescs;
	D3D12_RAYTRACING_INSTANCE_DESC* pInstanceDescs = nullptr;

	friend class PathIntegrator;
	friend class Picking;
};