#pragma once
#include <d3d12.h>
#include <wrl/client.h>
#include "RootSignature.h"

class Device;
class RaytracingPipelineStateProxy;

class RaytracingPipelineState
{
public:
	RaytracingPipelineState() = default;
	RaytracingPipelineState(const Device * pDevice, RaytracingPipelineStateProxy& Proxy);

	RaytracingPipelineState(RaytracingPipelineState&&) noexcept = default;
	RaytracingPipelineState& operator=(RaytracingPipelineState&&) noexcept = default;

	RaytracingPipelineState(const RaytracingPipelineState&) = delete;
	RaytracingPipelineState& operator=(const RaytracingPipelineState&) = delete;

	inline auto GetD3DPipelineState() const { return m_StateObject.Get(); }
	inline auto GetD3DPipelineStateProperties() const { return m_StateObjectProperties.Get(); }
private:
	Microsoft::WRL::ComPtr<ID3D12StateObject> m_StateObject;
	Microsoft::WRL::ComPtr<ID3D12StateObjectProperties> m_StateObjectProperties;
	RootSignature m_DummyGlobalRootSignature;
	RootSignature m_DummyLocalRootSignature;
};