#include "pch.h"
#include "RaytracingPipelineState.h"
#include "Device.h"
#include "../Proxy/RaytracingPipelineStateProxy.h"

//----------------------------------------------------------------------------------------------------
DXILLibrary::DXILLibrary(const Library* pLibrary, const std::vector<std::wstring>& Symbols)
	: pLibrary(pLibrary), Symbols(Symbols)
{
}

//----------------------------------------------------------------------------------------------------
HitGroup::HitGroup(LPCWSTR pHitGroupName, LPCWSTR pAnyHitSymbol, LPCWSTR pClosestHitSymbol, LPCWSTR pIntersectionSymbol)
	: HitGroupName(pHitGroupName ? pHitGroupName : L""),
	AnyHitSymbol(pAnyHitSymbol ? pAnyHitSymbol : L""),
	ClosestHitSymbol(pClosestHitSymbol ? pClosestHitSymbol : L""),
	IntersectionSymbol(pIntersectionSymbol ? pIntersectionSymbol : L"")
{
}

//----------------------------------------------------------------------------------------------------
RootSignatureAssociation::RootSignatureAssociation(const RootSignature* pRootSignature, const std::vector<std::wstring>& Symbols)
	: pRootSignature(pRootSignature), Symbols(Symbols)
{
}

//----------------------------------------------------------------------------------------------------
RaytracingPipelineState::RaytracingPipelineState(const Device* pDevice, RaytracingPipelineStateProxy& Proxy)
{
	Proxy.Link();
	pGlobalRootSignature = Proxy.m_pGlobalRootSignature;

	CD3DX12_STATE_OBJECT_DESC desc(D3D12_STATE_OBJECT_TYPE_RAYTRACING_PIPELINE);

	// Add all the DXIL libraries
	for (const auto& library : Proxy.m_Libraries)
	{
		auto pLibrarySubobject = desc.CreateSubobject<CD3DX12_DXIL_LIBRARY_SUBOBJECT>();

		D3D12_SHADER_BYTECODE shaderByteCode = library.pLibrary->GetD3DShaderBytecode();
		pLibrarySubobject->SetDXILLibrary(&shaderByteCode);

		for (const auto& symbol : library.Symbols)
		{
			pLibrarySubobject->DefineExport(symbol.data());
		}
	}

	// Add all the hit group declarations
	for (const auto& hitGroup : Proxy.m_HitGroups)
	{
		auto pHitgroupSubobject = desc.CreateSubobject<CD3DX12_HIT_GROUP_SUBOBJECT>();

		pHitgroupSubobject->SetHitGroupExport(hitGroup.HitGroupName.data());
		pHitgroupSubobject->SetHitGroupType(D3D12_HIT_GROUP_TYPE_TRIANGLES);

		LPCWSTR pAnyHitSymbolImport = hitGroup.AnyHitSymbol.empty() ? nullptr : hitGroup.AnyHitSymbol.data();
		pHitgroupSubobject->SetAnyHitShaderImport(pAnyHitSymbolImport);

		LPCWSTR pClosestHitShaderImport = hitGroup.ClosestHitSymbol.empty() ? nullptr : hitGroup.ClosestHitSymbol.data();
		pHitgroupSubobject->SetClosestHitShaderImport(pClosestHitShaderImport);

		LPCWSTR pIntersectionShaderImport = hitGroup.IntersectionSymbol.empty() ? nullptr : hitGroup.IntersectionSymbol.data();
		pHitgroupSubobject->SetIntersectionShaderImport(pIntersectionShaderImport);
	}

	// Add 2 subobjects for every root signature association
	for (auto& rootSignatureAssociation : Proxy.m_RootSignatureAssociations)
	{
		// The root signature association requires two objects for each: one to declare the root
		// signature, and another to associate that root signature to a set of symbols

		// Add a subobject to declare the local root signature
		auto pLocalRootSignatureSubobject = desc.CreateSubobject<CD3DX12_LOCAL_ROOT_SIGNATURE_SUBOBJECT>();

		pLocalRootSignatureSubobject->SetRootSignature(rootSignatureAssociation.pRootSignature->GetD3DRootSignature());

		// Add a subobject for the association between the exported shader symbols and the local root signature
		auto pAssociationSubobject = desc.CreateSubobject<CD3DX12_SUBOBJECT_TO_EXPORTS_ASSOCIATION_SUBOBJECT>();

		for (const auto& symbols : rootSignatureAssociation.Symbols)
		{
			pAssociationSubobject->AddExport(symbols.data());
		}
		pAssociationSubobject->SetSubobjectToAssociate(*pLocalRootSignatureSubobject);
	}

	// Add a subobject for global root signature
	auto pGlobalRootSignatureSubobject = desc.CreateSubobject<CD3DX12_GLOBAL_ROOT_SIGNATURE_SUBOBJECT>();
	pGlobalRootSignatureSubobject->SetRootSignature(Proxy.m_pGlobalRootSignature->GetD3DRootSignature());

	// Add a subobject for the raytracing shader config
	auto pShaderConfigSubobject = desc.CreateSubobject<CD3DX12_RAYTRACING_SHADER_CONFIG_SUBOBJECT>();
	pShaderConfigSubobject->Config(Proxy.m_ShaderConfig.MaxPayloadSizeInBytes, Proxy.m_ShaderConfig.MaxAttributeSizeInBytes);

	// Add a subobject for the association between shaders and the payload
	std::vector<std::wstring> exportedSymbols = Proxy.BuildShaderExportList();
	auto pAssociationSubobject = desc.CreateSubobject<CD3DX12_SUBOBJECT_TO_EXPORTS_ASSOCIATION_SUBOBJECT>();

	for (const auto& symbols : exportedSymbols)
	{
		pAssociationSubobject->AddExport(symbols.data());
	}
	pAssociationSubobject->SetSubobjectToAssociate(*pShaderConfigSubobject);

	// Add a subobject for the raytracing pipeline configuration
	auto pPipelineConfigSubobject = desc.CreateSubobject<CD3DX12_RAYTRACING_PIPELINE_CONFIG_SUBOBJECT>();
	pPipelineConfigSubobject->Config(Proxy.m_PipelineConfig.MaxTraceRecursionDepth);

	ThrowCOMIfFailed(pDevice->GetD3DDevice()->CreateStateObject(desc, IID_PPV_ARGS(m_StateObject.ReleaseAndGetAddressOf())));
	// Query the state object properties
	ThrowCOMIfFailed(m_StateObject->QueryInterface(IID_PPV_ARGS(m_StateObjectProperties.ReleaseAndGetAddressOf())));
}

ShaderIdentifier RaytracingPipelineState::GetShaderIdentifier(LPCWSTR pExportName)
{
	ShaderIdentifier shaderIdentifier = {};
	void* pShaderIdentifier = m_StateObjectProperties->GetShaderIdentifier(pExportName);
	if (!pShaderIdentifier)
	{
		using convert_type = std::codecvt_utf8<wchar_t>;
		std::wstring_convert<convert_type, wchar_t> converter;

		throw std::logic_error("Unknown shader identifier: " + converter.to_bytes(pExportName));
	}

	memcpy(shaderIdentifier.Data, pShaderIdentifier, sizeof(ShaderIdentifier));
	return shaderIdentifier;
}