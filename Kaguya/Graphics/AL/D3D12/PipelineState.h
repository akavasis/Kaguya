#pragma once
#include <d3d12.h>
#include <wrl/client.h>

class Device;

class PipelineState
{
public:
	enum class Type { Unknown, Graphics, Compute };

	PipelineState() = default;
	PipelineState(Type Type);
	virtual ~PipelineState();

	PipelineState(PipelineState&&) = default;
	PipelineState& operator=(PipelineState&&) = default;

	PipelineState(const PipelineState&) = delete;
	PipelineState& operator=(const PipelineState&) = delete;

	inline auto GetD3DPipelineState() const { return m_PipelineState.Get(); }
	inline auto GetType() const { return m_Type; }
protected:
	Microsoft::WRL::ComPtr<ID3D12PipelineState> m_PipelineState;
	Type m_Type = PipelineState::Type::Unknown;
};

class GraphicsPipelineState : public PipelineState
{
public:
	struct Properties
	{
		D3D12_GRAPHICS_PIPELINE_STATE_DESC Desc;
	};

	GraphicsPipelineState() = default;
	GraphicsPipelineState(Device* pDevice, const Properties& Properties);
	~GraphicsPipelineState() override;

	GraphicsPipelineState(GraphicsPipelineState&&) = default;
	GraphicsPipelineState& operator=(GraphicsPipelineState&&) = default;

	GraphicsPipelineState(const GraphicsPipelineState&) = delete;
	GraphicsPipelineState& operator=(const GraphicsPipelineState&) = delete;
};

class ComputePipelineState : public PipelineState
{
public:
	struct Properties
	{
		D3D12_COMPUTE_PIPELINE_STATE_DESC Desc;
	};

	ComputePipelineState() = default;
	ComputePipelineState(Device* pDevice, const Properties& Properties);
	~ComputePipelineState() override;

	ComputePipelineState(ComputePipelineState&&) = default;
	ComputePipelineState& operator=(ComputePipelineState&&) = default;

	ComputePipelineState(const ComputePipelineState&) = delete;
	ComputePipelineState& operator=(const ComputePipelineState&) = delete;
};