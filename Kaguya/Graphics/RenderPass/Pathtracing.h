#pragma once
#include "../RenderPass.h"

class Pathtracing : public RenderPass
{
public:
	enum EResources
	{
		RenderTarget,
		NumResources
	};

	struct Settings
	{
		UINT NumSamplesPerPixel	= 4;
		UINT MaxDepth			= 6;
	};

	Pathtracing(UINT Width, UINT Height);
	~Pathtracing() override;
protected:
	void InitializePipeline(RenderDevice* pRenderDevice) override;
	void ScheduleResource(ResourceScheduler* pResourceScheduler, RenderGraph* pRenderGraph) override;
	void InitializeScene(GpuScene* pGpuScene, RenderDevice* pRenderDevice) override;
	void RenderGui() override;
	void Execute(RenderContext& RenderContext, RenderGraph* pRenderGraph) override;
	void StateRefresh() override;
private:
	Settings settings;
	GpuScene* pGpuScene;

	Microsoft::WRL::ComPtr<ID3D12Resource>	m_RayGenerationShaderTable;
	Microsoft::WRL::ComPtr<ID3D12Resource>	m_MissShaderTable;
	Microsoft::WRL::ComPtr<ID3D12Resource>	m_HitGroupShaderTable;
	D3D12MA::Allocation*					m_RayGenerationShaderTableAllocation = nullptr;
	D3D12MA::Allocation*					m_MissShaderTableAllocation = nullptr;
	D3D12MA::Allocation*					m_HitGroupShaderTableAllocation = nullptr;

	D3D12_GPU_VIRTUAL_ADDRESS_RANGE				RayGenerationShaderRecord;
	D3D12_GPU_VIRTUAL_ADDRESS_RANGE_AND_STRIDE	MissShaderTable;
	D3D12_GPU_VIRTUAL_ADDRESS_RANGE_AND_STRIDE	HitGroupTable;

	struct RootArgument
	{
		D3D12_GPU_VIRTUAL_ADDRESS VertexBuffer;
		D3D12_GPU_VIRTUAL_ADDRESS IndexBuffer;
	};

	ShaderTable<RootArgument> HitGroupShaderTable;
};