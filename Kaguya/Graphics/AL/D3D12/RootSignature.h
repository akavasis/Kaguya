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
	RootSignature(Device* pDevice, RootSignatureProxy& Proxy);
	~RootSignature();

	RootSignature(RootSignature&&) noexcept;
	RootSignature& operator=(RootSignature&&) noexcept;

	RootSignature(const RootSignature&) = delete;
	RootSignature& operator=(const RootSignature&) = delete;

	inline auto GetD3DRootSignature() const { return m_RootSignature.Get(); }
	inline const auto& GetD3DRootSignatureDesc() const { return m_RootSignatureDesc; }
private:
	enum : UINT { MaxDescriptorTables = 32 };

	Microsoft::WRL::ComPtr<ID3D12RootSignature> m_RootSignature;

	D3D12_ROOT_SIGNATURE_DESC1 m_RootSignatureDesc;
};