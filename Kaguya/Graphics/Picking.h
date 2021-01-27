#pragma once
#include "RenderDevice.h"
#include "RaytracingAccelerationStructure.h"

class Picking
{
public:
	void Create(RenderDevice* pRenderDevice);

	void UpdateShaderTable(const RaytracingAccelerationStructure& Scene, CommandList& CommandList);
	void ShootPickingRay(D3D12_GPU_VIRTUAL_ADDRESS SystemConstants,
		const RaytracingAccelerationStructure& Scene, CommandList& CommandList);

	std::optional<Entity> GetSelectedEntity();
private:
	RenderDevice* pRenderDevice;

	RootSignature GlobalRS;
	RaytracingPipelineState RTPSO;

	std::shared_ptr<Resource> m_RayGenerationShaderTable;
	std::shared_ptr<Resource> m_MissShaderTable;
	std::shared_ptr<Resource> m_HitGroupShaderTable;

	std::shared_ptr<Resource> PickingResult;
	std::shared_ptr<Resource> PickingReadback;

	ShaderTable<void> RayGenerationShaderTable;
	ShaderTable<void> MissShaderTable;
	ShaderTable<void> HitGroupShaderTable;
	std::vector<Entity> Entities;
};