#pragma once
#include <d3d12.h>
#include <wrl/client.h>

class Device;

class PipelineState
{
public:
	enum class Type { Graphics, Compute };

	PipelineState(Type Type);
	virtual ~PipelineState();

	inline auto GetD3DPipelineState() const { return m_PipelineState.Get(); }
	inline auto GetType() const { return m_Type; }
protected:
	Microsoft::WRL::ComPtr<ID3D12PipelineState> m_PipelineState;
	Type m_Type;
};

class GraphicsPipelineState : public PipelineState
{
public:
	struct Properties
	{
		D3D12_GRAPHICS_PIPELINE_STATE_DESC Desc;
	};

	GraphicsPipelineState(Device* pDevice, const Properties& Properties);
	~GraphicsPipelineState() override;
};

class ComputePipelineState : public PipelineState
{
public:
	struct Properties
	{
		D3D12_COMPUTE_PIPELINE_STATE_DESC Desc;
	};

	ComputePipelineState(Device* pDevice, const Properties& Properties);
	~ComputePipelineState() override;
};