#include "pch.h"
#include "RaytracingPipelineStateProxy.h"

RaytracingPipelineStateProxy::RaytracingPipelineStateProxy()
{
	m_pGlobalRootSignature = nullptr;
	m_ShaderConfig = {};
	m_PipelineConfig = {};
}

void RaytracingPipelineStateProxy::AddLibrary(const Library* pLibrary, const std::vector<std::wstring>& Symbols)
{
	// Add a DXIL library to the pipeline. Note that this library has to be
	// compiled with dxc, using a lib_6_3 target. The exported symbols must correspond exactly to the
	// names of the shaders declared in the library, although unused ones can be omitted.
	m_Libraries.emplace_back(DXILLibrary(pLibrary, Symbols));
}

void RaytracingPipelineStateProxy::AddHitGroup(LPCWSTR pHitGroupName, LPCWSTR pAnyHitSymbol, LPCWSTR pClosestHitSymbol, LPCWSTR pIntersectionSymbol)
{
	// In DXR the hit-related shaders are grouped into hit groups. Such shaders are:
	// - The intersection shader, which can be used to intersect custom geometry, and is called upon
	//   hitting the bounding box the the object. A default one exists to intersect triangles
	// - The any hit shader, called on each intersection, which can be used to perform early
	//   alpha-testing and allow the ray to continue if needed. Default is a pass-through.
	// - The closest hit shader, invoked on the hit point closest to the ray start.
	// The shaders in a hit group share the same root signature, and are only referred to by the
	// hit group name in other places of the program.
	m_HitGroups.emplace_back(HitGroup(pHitGroupName, pAnyHitSymbol, pClosestHitSymbol, pIntersectionSymbol));

	// 3 different shaders can be invoked to obtain an intersection: an
	// intersection shader is called
	// when hitting the bounding box of non-triangular geometry. This is beyond
	// the scope of this tutorial. An any-hit shader is called on potential
	// intersections. This shader can, for example, perform alpha-testing and
	// discard some intersections. Finally, the closest-hit program is invoked on
	// the intersection point closest to the ray origin. Those 3 shaders are bound
	// together into a hit group.

	// Note that for triangular geometry the intersection shader is built-in. An
	// empty any-hit shader is also defined by default, so in our simple case each
	// hit group contains only the closest hit shader. Note that since the
	// exported symbols are defined above the shaders can be simply referred to by
	// name.
}

void RaytracingPipelineStateProxy::AddRootSignatureAssociation(const RootSignature* pRootSignature, const std::vector<std::wstring>& Symbols)
{
	m_RootSignatureAssociations.emplace_back(RootSignatureAssociation(pRootSignature, Symbols));
}

void RaytracingPipelineStateProxy::SetGlobalRootSignature(const RootSignature* pGlobalRootSignature)
{
	m_pGlobalRootSignature = pGlobalRootSignature;
}

void RaytracingPipelineStateProxy::SetRaytracingShaderConfig(UINT MaxPayloadSizeInBytes, UINT MaxAttributeSizeInBytes)
{
	m_ShaderConfig.MaxPayloadSizeInBytes = MaxPayloadSizeInBytes;
	m_ShaderConfig.MaxAttributeSizeInBytes = MaxAttributeSizeInBytes;
}

void RaytracingPipelineStateProxy::SetRaytracingPipelineConfig(UINT MaxTraceRecursionDepth)
{
	m_PipelineConfig.MaxTraceRecursionDepth = MaxTraceRecursionDepth;
}

void RaytracingPipelineStateProxy::Link()
{
	assert(m_pGlobalRootSignature != nullptr);
	assert(m_PipelineConfig.MaxTraceRecursionDepth >= 0 && m_PipelineConfig.MaxTraceRecursionDepth < 31);
}

std::vector<std::wstring> RaytracingPipelineStateProxy::BuildShaderExportList()
{
	// Get all names from libraries
	// Get names associated to hit groups
	// Return list of libraries + hit group names - shaders in hit groups
	std::unordered_set<std::wstring> exports;

	// Add all the symbols exported by the libraries
	for (const auto& library : m_Libraries)
	{
		for (const auto& symbol : library.Symbols)
		{
#ifdef _DEBUG
			// Sanity check in debug mode: check that no name is exported more than once
			if (exports.find(symbol) != exports.end())
			{
				throw std::logic_error("Multiple definition of a symbol in the imported DXIL libraries");
			}
#endif
			exports.insert(symbol);
		}
	}

#ifdef _DEBUG
	// Sanity check in debug mode: verify that the hit groups do not reference an unknown shader name
	{
		std::unordered_set<std::wstring> all_exports = exports;

		for (const auto& hitGroup : m_HitGroups)
		{
			if (!hitGroup.AnyHitSymbol.empty() &&
				exports.find(hitGroup.AnyHitSymbol) == exports.end())
			{
				throw std::logic_error("Any hit symbol not found in the imported DXIL libraries");
			}

			if (!hitGroup.ClosestHitSymbol.empty() &&
				exports.find(hitGroup.ClosestHitSymbol) == exports.end())
			{
				throw std::logic_error("Closest hit symbol not found in the imported DXIL libraries");
			}

			if (!hitGroup.IntersectionSymbol.empty() &&
				exports.find(hitGroup.IntersectionSymbol) == exports.end())
			{
				throw std::logic_error("Intersection symbol not found in the imported DXIL libraries");
			}

			all_exports.insert(hitGroup.HitGroupName);
		}

		// Sanity check in debug mode: verify that the root signature associations do not reference an
		// unknown shader or hit group name
		for (const auto& rootSignatureAssociation : m_RootSignatureAssociations)
		{
			for (const auto& symbol : rootSignatureAssociation.Symbols)
			{
				if (!symbol.empty() && all_exports.find(symbol) == all_exports.end())
				{
					throw std::logic_error("Root association symbol not found in the imported DXIL libraries and hit group names");
				}
			}
		}
	}
#endif

	// Go through all hit groups and remove the symbols corresponding to intersection, any hit and
	// closest hit shaders from the symbol set
	for (const auto& hitGroup : m_HitGroups)
	{
		if (!hitGroup.ClosestHitSymbol.empty())
		{
			exports.erase(hitGroup.ClosestHitSymbol);
		}
		if (!hitGroup.AnyHitSymbol.empty())
		{
			exports.erase(hitGroup.AnyHitSymbol);
		}
		if (!hitGroup.IntersectionSymbol.empty())
		{
			exports.erase(hitGroup.IntersectionSymbol);
		}
		exports.insert(hitGroup.HitGroupName);
	}

	// Finally build a vector containing ray generation and miss shaders, plus the hit group names
	std::vector<std::wstring> exportedSymbols;
	for (const auto& name : exports)
	{
		exportedSymbols.push_back(name);
	}

	return exportedSymbols;
}