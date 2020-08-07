#pragma once
#include <d3d12.h>
#include "Proxy.h"
#include "../D3D12/RaytracingPipelineState.h"

class RootSignature;
class Library;

class RaytracingPipelineStateProxy : public Proxy
{
public:
	friend class RaytracingPipelineState;
	RaytracingPipelineStateProxy();

	void AddLibrary(const Library* pLibrary, const std::vector<std::wstring>& Symbols);
	void AddHitGroup(LPCWSTR pHitGroupName, LPCWSTR pAnyHitSymbol, LPCWSTR pClosestHitSymbol, LPCWSTR pIntersectionSymbol);
	void AddRootSignatureAssociation(const RootSignature* pRootSignature, const std::vector<std::wstring>& Symbols);

	void SetRaytracingShaderConfig(UINT MaxPayloadSizeInBytes, UINT MaxAttributeSizeInBytes);
	void SetRaytracingPipelineConfig(UINT MaxTraceRecursionDepth);
protected:
	void Link() override;
private:
	std::vector<std::wstring> BuildShaderExportList();

	std::vector<DXILLibrary> m_Libraries;
	std::vector<HitGroup> m_HitGroups;
	std::vector<RootSignatureAssociation> m_RootSignatureAssociations;
	RaytracingPipelineState::ShaderConfig m_ShaderConfig;
	RaytracingPipelineState::PipelineConfig m_PipelineConfig;
};