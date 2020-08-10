#include "pch.h"
#include "RaytracingPipelineState.h"
#include "Device.h"
#include "../Proxy/RaytracingPipelineStateProxy.h"
#include "../Proxy/RootSignatureProxy.h"
#include <sstream>
#include <codecvt>

// From DirectX12 Sample
inline void PrintStateObjectDesc(const D3D12_STATE_OBJECT_DESC* desc)
{
	std::wstringstream wstr;
	wstr << L"\n";
	wstr << L"--------------------------------------------------------------------\n";
	wstr << L"| D3D12 State Object 0x" << static_cast<const void*>(desc) << L": ";
	if (desc->Type == D3D12_STATE_OBJECT_TYPE_COLLECTION) wstr << L"Collection\n";
	if (desc->Type == D3D12_STATE_OBJECT_TYPE_RAYTRACING_PIPELINE) wstr << L"Raytracing Pipeline\n";

	auto ExportTree = [](UINT depth, UINT numExports, const D3D12_EXPORT_DESC* exports)
	{
		std::wostringstream woss;
		for (UINT i = 0; i < numExports; i++)
		{
			woss << L"|";
			if (depth > 0)
			{
				for (UINT j = 0; j < 2 * depth - 1; j++) woss << L" ";
			}
			woss << L" [" << i << L"]: ";
			if (exports[i].ExportToRename) woss << exports[i].ExportToRename << L" --> ";
			woss << exports[i].Name << L"\n";
		}
		return woss.str();
	};

	for (UINT i = 0; i < desc->NumSubobjects; i++)
	{
		wstr << L"| Subobject: [" << i << L"]: ";
		switch (desc->pSubobjects[i].Type)
		{
		case D3D12_STATE_SUBOBJECT_TYPE_GLOBAL_ROOT_SIGNATURE:
			wstr << L"Global Root Signature 0x" << desc->pSubobjects[i].pDesc << L"\n";
			break;
		case D3D12_STATE_SUBOBJECT_TYPE_LOCAL_ROOT_SIGNATURE:
			wstr << L"Local Root Signature 0x" << desc->pSubobjects[i].pDesc << L"\n";
			break;
		case D3D12_STATE_SUBOBJECT_TYPE_NODE_MASK:
			wstr << L"Node Mask: 0x" << std::hex << std::setfill(L'0') << std::setw(8) << *static_cast<const UINT*>(desc->pSubobjects[i].pDesc) << std::setw(0) << std::dec << L"\n";
			break;
		case D3D12_STATE_SUBOBJECT_TYPE_DXIL_LIBRARY:
		{
			wstr << L"DXIL Library 0x";
			auto lib = static_cast<const D3D12_DXIL_LIBRARY_DESC*>(desc->pSubobjects[i].pDesc);
			wstr << lib->DXILLibrary.pShaderBytecode << L", " << lib->DXILLibrary.BytecodeLength << L" bytes\n";
			wstr << ExportTree(1, lib->NumExports, lib->pExports);
			break;
		}
		case D3D12_STATE_SUBOBJECT_TYPE_EXISTING_COLLECTION:
		{
			wstr << L"Existing Library 0x";
			auto collection = static_cast<const D3D12_EXISTING_COLLECTION_DESC*>(desc->pSubobjects[i].pDesc);
			wstr << collection->pExistingCollection << L"\n";
			wstr << ExportTree(1, collection->NumExports, collection->pExports);
			break;
		}
		case D3D12_STATE_SUBOBJECT_TYPE_SUBOBJECT_TO_EXPORTS_ASSOCIATION:
		{
			wstr << L"Subobject to Exports Association (Subobject [";
			auto association = static_cast<const D3D12_SUBOBJECT_TO_EXPORTS_ASSOCIATION*>(desc->pSubobjects[i].pDesc);
			UINT index = static_cast<UINT>(association->pSubobjectToAssociate - desc->pSubobjects);
			wstr << index << L"])\n";
			for (UINT j = 0; j < association->NumExports; j++)
			{
				wstr << L"|  [" << j << L"]: " << association->pExports[j] << L"\n";
			}
			break;
		}
		case D3D12_STATE_SUBOBJECT_TYPE_DXIL_SUBOBJECT_TO_EXPORTS_ASSOCIATION:
		{
			wstr << L"DXIL Subobjects to Exports Association (";
			auto association = static_cast<const D3D12_DXIL_SUBOBJECT_TO_EXPORTS_ASSOCIATION*>(desc->pSubobjects[i].pDesc);
			wstr << association->SubobjectToAssociate << L")\n";
			for (UINT j = 0; j < association->NumExports; j++)
			{
				wstr << L"|  [" << j << L"]: " << association->pExports[j] << L"\n";
			}
			break;
		}
		case D3D12_STATE_SUBOBJECT_TYPE_RAYTRACING_SHADER_CONFIG:
		{
			wstr << L"Raytracing Shader Config\n";
			auto config = static_cast<const D3D12_RAYTRACING_SHADER_CONFIG*>(desc->pSubobjects[i].pDesc);
			wstr << L"|  [0]: Max Payload Size: " << config->MaxPayloadSizeInBytes << L" bytes\n";
			wstr << L"|  [1]: Max Attribute Size: " << config->MaxAttributeSizeInBytes << L" bytes\n";
			break;
		}
		case D3D12_STATE_SUBOBJECT_TYPE_RAYTRACING_PIPELINE_CONFIG:
		{
			wstr << L"Raytracing Pipeline Config\n";
			auto config = static_cast<const D3D12_RAYTRACING_PIPELINE_CONFIG*>(desc->pSubobjects[i].pDesc);
			wstr << L"|  [0]: Max Recursion Depth: " << config->MaxTraceRecursionDepth << L"\n";
			break;
		}
		case D3D12_STATE_SUBOBJECT_TYPE_HIT_GROUP:
		{
			wstr << L"Hit Group (";
			auto hitGroup = static_cast<const D3D12_HIT_GROUP_DESC*>(desc->pSubobjects[i].pDesc);
			wstr << (hitGroup->HitGroupExport ? hitGroup->HitGroupExport : L"[none]") << L")\n";
			wstr << L"|  [0]: Any Hit Import: " << (hitGroup->AnyHitShaderImport ? hitGroup->AnyHitShaderImport : L"[none]") << L"\n";
			wstr << L"|  [1]: Closest Hit Import: " << (hitGroup->ClosestHitShaderImport ? hitGroup->ClosestHitShaderImport : L"[none]") << L"\n";
			wstr << L"|  [2]: Intersection Import: " << (hitGroup->IntersectionShaderImport ? hitGroup->IntersectionShaderImport : L"[none]") << L"\n";
			break;
		}
		}
		wstr << L"|--------------------------------------------------------------------\n";
	}
	wstr << L"\n";

	using convert_type = std::codecvt_utf8<wchar_t>;
	std::wstring_convert<convert_type, wchar_t> converter;

	std::string str = converter.to_bytes(wstr.str());
	CORE_INFO(str);
}

DXILLibrary::DXILLibrary(const Library* pLibrary, const std::vector<std::wstring>& Symbols)
	: pLibrary(pLibrary), Symbols(Symbols)
{
}

HitGroup::HitGroup(LPCWSTR pHitGroupName, LPCWSTR pAnyHitSymbol, LPCWSTR pClosestHitSymbol, LPCWSTR pIntersectionSymbol)
	: HitGroupName(pHitGroupName ? pHitGroupName : L""),
	AnyHitSymbol(pAnyHitSymbol ? pAnyHitSymbol : L""),
	ClosestHitSymbol(pClosestHitSymbol ? pClosestHitSymbol : L""),
	IntersectionSymbol(pIntersectionSymbol ? pIntersectionSymbol : L"")
{
}

RootSignatureAssociation::RootSignatureAssociation(const RootSignature* pRootSignature, const std::vector<std::wstring>& Symbols)
	: pRootSignature(pRootSignature), Symbols(Symbols)
{
}

RaytracingPipelineState::RaytracingPipelineState(const Device* pDevice, RaytracingPipelineStateProxy& Proxy)
{
	Proxy.Link();

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

	// Build a list of all the symbols for ray generation, miss and hit groups
	// Those shaders have to be associated with the payload definition
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

	PrintStateObjectDesc(desc);
	ThrowCOMIfFailed(pDevice->GetD3DDevice()->CreateStateObject(desc, IID_PPV_ARGS(m_StateObject.ReleaseAndGetAddressOf())));
	// Query the state object properties
	ThrowCOMIfFailed(m_StateObject->QueryInterface(IID_PPV_ARGS(m_StateObjectProperties.ReleaseAndGetAddressOf())));
}

ShaderIdentifier RaytracingPipelineState::GetShaderIdentifier(const std::wstring& ExportName)
{
	ShaderIdentifier shaderIdentifier = {};
	void* pShaderIdentifier = m_StateObjectProperties->GetShaderIdentifier(ExportName.data());
	if (!pShaderIdentifier)
	{
		std::wstring errMsg(std::wstring(L"Unknown shader identifier: ") + ExportName);
		throw std::logic_error(std::string(errMsg.begin(), errMsg.end()));
	}

	memcpy(shaderIdentifier.Data, pShaderIdentifier, sizeof(ShaderIdentifier));
	return shaderIdentifier;
}