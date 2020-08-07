#pragma once
#include <d3d12.h>
#include <wrl/client.h>

class Device;
class RootSignatureProxy;

class RootSignature
{
public:
	enum : UINT { UnboundDescriptorSize = UINT_MAX };

	RootSignature() = default;
	RootSignature(const Device* pDevice, RootSignatureProxy& Proxy);

	RootSignature(RootSignature&&) noexcept = default;
	RootSignature& operator=(RootSignature&&) noexcept = default;

	RootSignature(const RootSignature&) = delete;
	RootSignature& operator=(const RootSignature&) = delete;

	inline auto GetD3DRootSignature() const { return m_RootSignature.Get(); }
private:
	Microsoft::WRL::ComPtr<ID3D12RootSignature> m_RootSignature;
};