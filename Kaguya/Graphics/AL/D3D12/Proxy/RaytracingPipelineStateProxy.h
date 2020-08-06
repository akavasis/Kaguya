#pragma once
#include <d3d12.h>
#include "Proxy.h"

class Library;
class RootSignature;

class RaytracingPipelineStateProxy : public Proxy
{
	friend class RaytracingPipelineState;
public:
	RaytracingPipelineStateProxy();

	void AddLibrary(Library* pLibrary, const std::vector<std::wstring>& Symbols);
	void AddHitGroup(LPCWSTR pHitGroupName, LPCWSTR pAnyHitSymbol, LPCWSTR pClosestHitSymbol, LPCWSTR pIntersectionSymbol);
	void AddRootSignatureAssociation(RootSignature* pRootSignature, const std::vector<std::wstring>& Symbols);

	void SetRaytracingShaderConfig(UINT MaxPayloadSizeInBytes, UINT MaxAttributeSizeInBytes);
	void SetRaytracingPipelineConfig(UINT MaxTraceRecursionDepth);
protected:
	void Link() override;
private:
	std::vector<std::wstring> BuildShaderExportList();

	struct DXILLibrary
	{
		DXILLibrary(Library* pLibrary, const std::vector<std::wstring>& Symbols);
		DXILLibrary(const DXILLibrary& rhs)
			: DXILLibrary(rhs.pLibrary, rhs.Symbols)
		{}

		Library* pLibrary;
		const std::vector<std::wstring> Symbols;

		std::vector<D3D12_EXPORT_DESC> Exports;
		D3D12_DXIL_LIBRARY_DESC Desc;
	};

	struct HitGroup
	{
		HitGroup(LPCWSTR pHitGroupName, LPCWSTR pAnyHitSymbol, LPCWSTR pClosestHitSymbol, LPCWSTR pIntersectionSymbol);
		HitGroup(const HitGroup& rhs)
			: HitGroup(rhs.HitGroupName.empty() ? nullptr : rhs.HitGroupName.data(),
				rhs.AnyHitSymbol.empty() ? nullptr : rhs.AnyHitSymbol.data(),
				rhs.ClosestHitSymbol.empty() ? nullptr : rhs.ClosestHitSymbol.data(),
				rhs.IntersectionSymbol.empty() ? nullptr : rhs.IntersectionSymbol.data())
		{}

		std::wstring HitGroupName;
		std::wstring AnyHitSymbol;
		std::wstring ClosestHitSymbol;
		std::wstring IntersectionSymbol;
		D3D12_HIT_GROUP_DESC Desc;
	};

	struct RootSignatureAssociation
	{
		RootSignatureAssociation(RootSignature* pRootSignature, const std::vector<std::wstring>& Symbols);
		RootSignatureAssociation(const RootSignatureAssociation& rhs)
			: RootSignatureAssociation(rhs.pRootSignature, rhs.Symbols)
		{}

		RootSignature* pRootSignature;
		std::vector<std::wstring> Symbols;
		std::vector<LPCWSTR> SymbolPtrs;
		D3D12_LOCAL_ROOT_SIGNATURE pLocalRootSignature;
		D3D12_SUBOBJECT_TO_EXPORTS_ASSOCIATION Association;
	};

	std::vector<DXILLibrary> m_Libraries;
	std::vector<HitGroup> m_HitGroups;
	std::vector<RootSignatureAssociation> m_RootSignatureAssociations;
	D3D12_RAYTRACING_SHADER_CONFIG m_RaytracingShaderConfig;
	D3D12_RAYTRACING_PIPELINE_CONFIG m_RaytracingPipelineConfig;
};