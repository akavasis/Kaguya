#include "pch.h"
#include "RaytracingPipelineState.h"
#include "Device.h"
#include "Proxy/RaytracingPipelineStateProxy.h"
#include "Proxy/RootSignatureProxy.h"
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

RaytracingPipelineState::RaytracingPipelineState(const Device* pDevice, RaytracingPipelineStateProxy& Proxy)
{
	m_DummyGlobalRootSignature = RootSignature(pDevice, RootSignatureProxy());

	RootSignatureProxy rsProxy;
	rsProxy.SetAsLocalRootSignature();
	m_DummyLocalRootSignature = RootSignature(pDevice, rsProxy);

	Proxy.Link();

	// Build a list of all the symbols for ray generation, miss and hit groups
	// Those shaders have to be associated with the payload definition
	std::vector<std::wstring> exportedSymbols = Proxy.BuildShaderExportList();
	std::vector<LPCWSTR> exportedSymbolPointers;

	exportedSymbolPointers.reserve(exportedSymbols.size());
	for (const auto& exportedSymbol : exportedSymbols)
	{
		exportedSymbolPointers.push_back(exportedSymbol.data());
	}

	UINT64 numSubobjects =
		Proxy.m_Libraries.size() +						// DXIL libraries
		Proxy.m_HitGroups.size() +						// Hit group declarations
		1 +												// Shader configuration
		1 +												// Shader payload
		2 * Proxy.m_RootSignatureAssociations.size() +	// Root signature declaration + association
		2 +												// Empty global and local root signatures
		1;												// Final pipeline subobject

	// Initialize a vector with the target object count. It is necessary to make the allocation before
	// adding subobjects as some subobjects reference other subobjects by pointer. Using push_back may
	// reallocate the array and invalidate those pointers.
	std::vector<D3D12_STATE_SUBOBJECT> subobjects(numSubobjects);

	size_t i = 0;

	// Add all the DXIL libraries
	for (const auto& library : Proxy.m_Libraries)
	{
		D3D12_STATE_SUBOBJECT librarySubobject = {};
		librarySubobject.Type = D3D12_STATE_SUBOBJECT_TYPE_DXIL_LIBRARY;
		librarySubobject.pDesc = &library.Desc;
		subobjects[i++] = librarySubobject;
	}

	// Add all the hit group declarations
	for (const auto& hitGroup : Proxy.m_HitGroups)
	{
		D3D12_STATE_SUBOBJECT hitGroupSubobject = {};
		hitGroupSubobject.Type = D3D12_STATE_SUBOBJECT_TYPE_HIT_GROUP;
		hitGroupSubobject.pDesc = &hitGroup.Desc;
		subobjects[i++] = hitGroupSubobject;
	}

	// Add 2 subobjects for every root signature association
	for (auto& rootSignatureAssociation : Proxy.m_RootSignatureAssociations)
	{
		// The root signature association requires two objects for each: one to declare the root
		// signature, and another to associate that root signature to a set of symbols

		// Add a subobject to declare the local root signature
		D3D12_STATE_SUBOBJECT localRootSignatureSubobject = {};
		localRootSignatureSubobject.Type = D3D12_STATE_SUBOBJECT_TYPE_LOCAL_ROOT_SIGNATURE;
		localRootSignatureSubobject.pDesc = &rootSignatureAssociation.pLocalRootSignature;
		subobjects[i++] = localRootSignatureSubobject;

		rootSignatureAssociation.Association.NumExports = static_cast<UINT>(rootSignatureAssociation.SymbolPtrs.size());
		rootSignatureAssociation.Association.pExports = rootSignatureAssociation.SymbolPtrs.data();
		rootSignatureAssociation.Association.pSubobjectToAssociate = &subobjects[i - 1];

		// Add a subobject for the association between the exported shader symbols and the local root signature
		D3D12_STATE_SUBOBJECT localRootSignatureAssociationSubobject = {};
		localRootSignatureAssociationSubobject.Type = D3D12_STATE_SUBOBJECT_TYPE_SUBOBJECT_TO_EXPORTS_ASSOCIATION;
		localRootSignatureAssociationSubobject.pDesc = &rootSignatureAssociation.Association;
		subobjects[i++] = localRootSignatureAssociationSubobject;
	}

	// The pipeline construction always requires an empty global root signature
	D3D12_GLOBAL_ROOT_SIGNATURE globalRootSignature;
	globalRootSignature.pGlobalRootSignature = m_DummyGlobalRootSignature.GetD3DRootSignature();

	D3D12_STATE_SUBOBJECT globalRootSignatureSubobject = {};
	globalRootSignatureSubobject.Type = D3D12_STATE_SUBOBJECT_TYPE_GLOBAL_ROOT_SIGNATURE;
	globalRootSignatureSubobject.pDesc = &globalRootSignature;
	subobjects[i++] = globalRootSignatureSubobject;

	// The pipeline construction always requires an empty local root signature
	D3D12_LOCAL_ROOT_SIGNATURE localRootSignature;
	localRootSignature.pLocalRootSignature = m_DummyLocalRootSignature.GetD3DRootSignature();

	D3D12_STATE_SUBOBJECT localRootSignatureSubobject = {};
	localRootSignatureSubobject.Type = D3D12_STATE_SUBOBJECT_TYPE_LOCAL_ROOT_SIGNATURE;
	localRootSignatureSubobject.pDesc = &localRootSignature;
	subobjects[i++] = localRootSignatureSubobject;

	// Add a subobject for the raytracing shader config
	D3D12_STATE_SUBOBJECT shaderConfigSubobject = {};
	shaderConfigSubobject.Type = D3D12_STATE_SUBOBJECT_TYPE_RAYTRACING_SHADER_CONFIG;
	shaderConfigSubobject.pDesc = &Proxy.m_RaytracingShaderConfig;
	subobjects[i++] = shaderConfigSubobject;

	// Add a subobject for the association between shaders and the payload
	D3D12_SUBOBJECT_TO_EXPORTS_ASSOCIATION shaderPayloadAssociation = {};
	shaderPayloadAssociation.NumExports = static_cast<UINT>(exportedSymbolPointers.size());
	shaderPayloadAssociation.pExports = exportedSymbolPointers.data();
	shaderPayloadAssociation.pSubobjectToAssociate = &subobjects[i - 1];

	D3D12_STATE_SUBOBJECT shaderPayloadAssociationSubobject = {};
	shaderPayloadAssociationSubobject.Type = D3D12_STATE_SUBOBJECT_TYPE_SUBOBJECT_TO_EXPORTS_ASSOCIATION;
	shaderPayloadAssociationSubobject.pDesc = &shaderPayloadAssociation;
	subobjects[i++] = shaderPayloadAssociationSubobject;

	// Add a subobject for the raytracing pipeline configuration
	D3D12_STATE_SUBOBJECT raytracingPipelineConfigSubobject = {};
	raytracingPipelineConfigSubobject.Type = D3D12_STATE_SUBOBJECT_TYPE_RAYTRACING_PIPELINE_CONFIG;
	raytracingPipelineConfigSubobject.pDesc = &Proxy.m_RaytracingPipelineConfig;
	subobjects[i++] = raytracingPipelineConfigSubobject;

	// Describe the ray tracing pipeline state object
	D3D12_STATE_OBJECT_DESC desc = {};
	desc.Type = D3D12_STATE_OBJECT_TYPE_RAYTRACING_PIPELINE;
	desc.NumSubobjects = i; // static_cast<UINT>(subobjects.size());
	desc.pSubobjects = subobjects.data();

	PrintStateObjectDesc(&desc);
	ThrowCOMIfFailed(pDevice->GetD3DDevice()->CreateStateObject(&desc, IID_PPV_ARGS(m_StateObject.ReleaseAndGetAddressOf())));
	// Query the state object properties
	ThrowCOMIfFailed(m_StateObject->QueryInterface(IID_PPV_ARGS(m_StateObjectProperties.ReleaseAndGetAddressOf())));
}