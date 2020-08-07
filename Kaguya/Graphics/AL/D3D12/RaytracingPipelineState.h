#pragma once
#include <d3d12.h>
#include <wrl/client.h>
#include "RootSignature.h"
#include "Library.h"

class Device;
class RaytracingPipelineStateProxy;

struct DXILLibrary
{
	DXILLibrary(const Library* pLibrary, const std::vector<std::wstring>& Symbols);

	const Library* pLibrary;
	const std::vector<std::wstring> Symbols;
};

struct HitGroup
{
	HitGroup(LPCWSTR pHitGroupName, LPCWSTR pAnyHitSymbol, LPCWSTR pClosestHitSymbol, LPCWSTR pIntersectionSymbol);

	std::wstring HitGroupName;
	std::wstring AnyHitSymbol;
	std::wstring ClosestHitSymbol;
	std::wstring IntersectionSymbol;
};

struct RootSignatureAssociation
{
	RootSignatureAssociation(const RootSignature* pRootSignature, const std::vector<std::wstring>& Symbols);

	const RootSignature* pRootSignature;
	std::vector<std::wstring> Symbols;
};

class RaytracingPipelineState
{
public:
	struct ShaderConfig
	{
		UINT MaxPayloadSizeInBytes;
		UINT MaxAttributeSizeInBytes;
	};

	struct PipelineConfig
	{
		UINT MaxTraceRecursionDepth;
	};

	RaytracingPipelineState() = default;
	RaytracingPipelineState(const Device* pDevice, RaytracingPipelineStateProxy& Proxy);

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