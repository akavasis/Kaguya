#pragma once
#include "RenderDevice.h"
#include "RaytracingAccelerationStructure.h"

class PathIntegrator
{
public:
	static constexpr UINT NumHitGroups = 1;

	void Create();

	void SetResolution(UINT Width, UINT Height);

	void RenderGui();

	void Reset();

	void UpdateShaderTable(const RaytracingAccelerationStructure& RaytracingAccelerationStructure,
		CommandList& CommandList);

	void Render(D3D12_GPU_VIRTUAL_ADDRESS SystemConstants,
		const RaytracingAccelerationStructure& RaytracingAccelerationStructure,
		D3D12_GPU_VIRTUAL_ADDRESS Materials,
		CommandList& CommandList);

	auto GetSRV() const
	{
		return SRV;
	}

	auto GetRenderTarget() const
	{
		return m_RenderTarget->pResource.Get();
	}
private:
	UINT Width = 0, Height = 0;

	RootSignature GlobalRS, LocalHitGroupRS;
	RaytracingPipelineState RTPSO;
	Descriptor UAV;
	Descriptor SRV;

	std::shared_ptr<Resource> m_RenderTarget;
	std::shared_ptr<Resource> m_RayGenerationShaderTable;
	std::shared_ptr<Resource> m_MissShaderTable;
	std::shared_ptr<Resource> m_HitGroupShaderTable;

	// Pad local root arguments explicitly
	struct RootArgument
	{
		UINT64 MaterialIndex : 32;
		UINT64 Padding : 32;
		D3D12_GPU_VIRTUAL_ADDRESS VertexBuffer;
		D3D12_GPU_VIRTUAL_ADDRESS IndexBuffer;
	};

	ShaderTable<void> RayGenerationShaderTable;
	ShaderTable<void> MissShaderTable;
	ShaderTable<RootArgument> HitGroupShaderTable;
};