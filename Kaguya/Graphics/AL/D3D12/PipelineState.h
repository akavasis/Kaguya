#pragma once
#include <d3d12.h>
#include <wrl/client.h>
#include "RootSignature.h"

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

	inline auto GetApiHandle() const { return m_PipelineState.Get(); }
	inline auto GetType() const { return m_Type; }

	const RootSignature* pRootSignature;
protected:
	Microsoft::WRL::ComPtr<ID3D12PipelineState> m_PipelineState;
	Type m_Type;
};

class GraphicsPipelineState : public PipelineState
{
public:
	GraphicsPipelineState() = default;
	GraphicsPipelineState(const Device* pDevice, GraphicsPipelineStateProxy& Proxy);
};

class ComputePipelineState : public PipelineState
{
public:
	ComputePipelineState() = default;
	ComputePipelineState(const Device* pDevice, ComputePipelineStateProxy& Proxy);
};