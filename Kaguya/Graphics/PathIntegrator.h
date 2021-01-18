#pragma once
#include "RenderDevice.h"
#include "RTScene.h"

class PathIntegrator
{
public:
	PathIntegrator();

	void Create(RenderDevice* pRenderDevice, RTScene* pRTScene);

	void SetResolution(UINT Width, UINT Height);

	void RenderGui();
	void Render(CommandList& CommandList);

private:
	RenderDevice* pRenderDevice;
	RTScene* pRTScene;

	UINT Width = 0, Height = 0;

	RootSignature GlobalRS, LocalHitGroupRS;
	RaytracingPipelineState RTPSO;
	Descriptor UAV;

	Resource m_RenderTarget;
	Resource m_RayGenerationShaderTable;
	Resource m_MissShaderTable;
	Resource m_HitGroupShaderTable;

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