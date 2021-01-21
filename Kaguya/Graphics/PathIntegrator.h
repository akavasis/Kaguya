#pragma once
#include "RenderDevice.h"
#include "RTScene.h"

class PathIntegrator
{
public:
	static constexpr UINT NumHitGroups = 1;

	PathIntegrator();

	void Create(RenderDevice* pRenderDevice);

	void SetResolution(UINT Width, UINT Height);

	void RenderGui();

	void UpdateShaderTable(const RaytracingAccelerationStructure& RaytracingAccelerationStructure, CommandList& CommandList);
	void Render(const RaytracingAccelerationStructure& RaytracingAccelerationStructure, CommandList& CommandList);
private:
	RenderDevice* pRenderDevice;

	UINT Width = 0, Height = 0;

	RootSignature GlobalRS, LocalHitGroupRS;
	RaytracingPipelineState RTPSO;
	Descriptor UAV;

	std::shared_ptr<Resource> m_RenderTarget;
	std::shared_ptr<Resource> m_RayGenerationShaderTable;
	std::shared_ptr<Resource> m_MissShaderTable;
	std::shared_ptr<Resource> m_HitGroupShaderTable;

	D3D12_GPU_VIRTUAL_ADDRESS_RANGE RayGenerationShaderRecord;
	D3D12_GPU_VIRTUAL_ADDRESS_RANGE_AND_STRIDE MissShaderTable;
	D3D12_GPU_VIRTUAL_ADDRESS_RANGE_AND_STRIDE HitGroupTable;

	struct RootArgument
	{
		D3D12_GPU_VIRTUAL_ADDRESS VertexBuffer;
		D3D12_GPU_VIRTUAL_ADDRESS IndexBuffer;
	};

	ShaderTable<RootArgument> HitGroupShaderTable;
};