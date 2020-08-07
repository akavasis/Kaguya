#pragma once
#include <d3d12.h>
#include <wrl/client.h>

class Device;
class PipelineStateProxy;
class GraphicsPipelineStateProxy;
class ComputePipelineStateProxy;

class PipelineState
{
public:
	enum class Type
	{
		Unknown,
		Graphics,
		Compute
	};

	PipelineState() = default;
	PipelineState(PipelineStateProxy& Proxy);
	virtual ~PipelineState() = default;

	PipelineState(PipelineState&&) noexcept = default;
	PipelineState& operator=(PipelineState&&) noexcept = default;

	PipelineState(const PipelineState&) = delete;
	PipelineState& operator=(const PipelineState&) = delete;

	inline auto GetD3DPipelineState() const { return m_PipelineState.Get(); }
	inline auto GetType() const { return m_Type; }
protected:
	Microsoft::WRL::ComPtr<ID3D12PipelineState> m_PipelineState;
	Type m_Type;
};

class GraphicsPipelineState : public PipelineState
{
public:
	GraphicsPipelineState() = default;
	GraphicsPipelineState(const Device* pDevice, GraphicsPipelineStateProxy& Proxy);

	GraphicsPipelineState(GraphicsPipelineState&&) noexcept = default;
	GraphicsPipelineState& operator=(GraphicsPipelineState&&) noexcept = default;

	GraphicsPipelineState(const GraphicsPipelineState&) = delete;
	GraphicsPipelineState& operator=(const GraphicsPipelineState&) = delete;
};

class ComputePipelineState : public PipelineState
{
public:
	ComputePipelineState() = default;
	ComputePipelineState(const Device* pDevice, ComputePipelineStateProxy& Proxy);

	ComputePipelineState(ComputePipelineState&&) noexcept = default;
	ComputePipelineState& operator=(ComputePipelineState&&) noexcept = default;

	ComputePipelineState(const ComputePipelineState&) = delete;
	ComputePipelineState& operator=(const ComputePipelineState&) = delete;
};