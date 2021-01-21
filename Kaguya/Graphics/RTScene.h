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
	void Create(RenderDevice* pRenderDevice, UINT NumHitGroups);

	operator auto() const
	{
		return TLASResult->pResource.Get();
	}

	void Clear();

	bool Empty() const
	{
		return TopLevelAccelerationStructure.Empty();
	}

	void AddInstance(MeshRenderer* pMeshRenderer);

	void Build(CommandList& CommandList);
private:
	RenderDevice* pRenderDevice = nullptr;
	UINT NumHitGroups = 0;

	RootSignature RS;
	PipelineState PSO;

	TopLevelAccelerationStructure TopLevelAccelerationStructure;
	UINT InstanceContributionToHitGroupIndex = 0;

	std::shared_ptr<Resource> TLASScratch, TLASResult;
};