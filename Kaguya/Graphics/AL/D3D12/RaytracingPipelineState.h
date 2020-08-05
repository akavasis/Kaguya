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
	RaytracingPipelineState(Device * pDevice, RaytracingPipelineStateProxy& Proxy);
	~RaytracingPipelineState() = default;

	RaytracingPipelineState(RaytracingPipelineState&&) noexcept = default;
	RaytracingPipelineState& operator=(RaytracingPipelineState&&) noexcept = default;

	RaytracingPipelineState(const RaytracingPipelineState&) = delete;
	RaytracingPipelineState& operator=(const RaytracingPipelineState&) = delete;

	inline auto GetD3DPipelineState() const { return m_StateObject.Get(); }
private:
	Microsoft::WRL::ComPtr<ID3D12StateObject> m_StateObject;
	RootSignature m_DummyGlobalRootSignature;
	RootSignature m_DummyLocalRootSignature;
};