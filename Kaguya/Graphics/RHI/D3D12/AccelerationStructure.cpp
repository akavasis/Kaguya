#include "pch.h"
#include "AccelerationStructure.h"

BottomLevelAccelerationStructure::BottomLevelAccelerationStructure()
{
	ScratchSizeInBytes = ResultSizeInBytes = 0;
}

void BottomLevelAccelerationStructure::AddGeometry(const D3D12_RAYTRACING_GEOMETRY_DESC& Desc)
{
	RaytracingGeometryDescs.push_back(Desc);
}

void BottomLevelAccelerationStructure::ComputeMemoryRequirements(ID3D12Device5* pDevice, UINT64* pScratchSizeInBytes, UINT64* pResultSizeInBytes)
{
	D3D12_BUILD_RAYTRACING_ACCELERATION_STRUCTURE_INPUTS Desc	= {};
	Desc.Type													= D3D12_RAYTRACING_ACCELERATION_STRUCTURE_TYPE_BOTTOM_LEVEL;
	Desc.Flags													= D3D12_RAYTRACING_ACCELERATION_STRUCTURE_BUILD_FLAG_PREFER_FAST_TRACE;
	Desc.NumDescs												= static_cast<UINT>(RaytracingGeometryDescs.size());
	Desc.DescsLayout											= D3D12_ELEMENTS_LAYOUT_ARRAY;
	Desc.pGeometryDescs											= RaytracingGeometryDescs.data();

	D3D12_RAYTRACING_ACCELERATION_STRUCTURE_PREBUILD_INFO PrebuildInfo = {};
	pDevice->GetRaytracingAccelerationStructurePrebuildInfo(&Desc, &PrebuildInfo);

	// Buffer sizes need to be 256-byte-aligned
	ScratchSizeInBytes		= AlignUp<UINT64>(PrebuildInfo.ScratchDataSizeInBytes, D3D12_RAYTRACING_ACCELERATION_STRUCTURE_BYTE_ALIGNMENT);
	ResultSizeInBytes		= AlignUp<UINT64>(PrebuildInfo.ResultDataMaxSizeInBytes, D3D12_RAYTRACING_ACCELERATION_STRUCTURE_BYTE_ALIGNMENT);

	*pScratchSizeInBytes	= ScratchSizeInBytes;
	*pResultSizeInBytes		= ResultSizeInBytes;
}

void BottomLevelAccelerationStructure::Generate(ID3D12GraphicsCommandList6* pCommandList, ID3D12Resource* pScratch, ID3D12Resource* pResult)
{
	if (ScratchSizeInBytes == 0 || ResultSizeInBytes == 0)
	{
		throw std::logic_error("Invalid Result and Scratch buffer sizes - ComputeMemoryRequirements needs to be called before Build");
	}

	if (!pResult || !pScratch)
	{
		throw std::logic_error("Invalid pResult and pScratch buffers");
	}

	// Create a descriptor of the requested builder work, to generate a
	// bottom-level AS from the input parameters
	D3D12_BUILD_RAYTRACING_ACCELERATION_STRUCTURE_DESC Desc = {};
	Desc.DestAccelerationStructureData						= pResult->GetGPUVirtualAddress();
	Desc.Inputs.Type										= D3D12_RAYTRACING_ACCELERATION_STRUCTURE_TYPE_BOTTOM_LEVEL;
	Desc.Inputs.Flags										= D3D12_RAYTRACING_ACCELERATION_STRUCTURE_BUILD_FLAG_PREFER_FAST_TRACE;
	Desc.Inputs.NumDescs									= static_cast<UINT>(RaytracingGeometryDescs.size());
	Desc.Inputs.DescsLayout									= D3D12_ELEMENTS_LAYOUT_ARRAY;
	Desc.Inputs.pGeometryDescs								= RaytracingGeometryDescs.data();
	Desc.SourceAccelerationStructureData					= NULL;
	Desc.ScratchAccelerationStructureData					= pScratch->GetGPUVirtualAddress();

	// Build the AS
	pCommandList->BuildRaytracingAccelerationStructure(&Desc, 0, nullptr);
}

TopLevelAccelerationStructure::TopLevelAccelerationStructure()
{
	ScratchSizeInBytes = ResultSizeInBytes = 0;
}

void TopLevelAccelerationStructure::AddInstance(const D3D12_RAYTRACING_INSTANCE_DESC& Desc)
{
	RaytracingInstanceDescs.push_back(Desc);
}

void TopLevelAccelerationStructure::ComputeMemoryRequirements(ID3D12Device5* pDevice, UINT64* pScratchSizeInBytes, UINT64* pResultSizeInBytes)
{
	// Describe the work being requested, in this case the construction of a
	// (possibly dynamic) top-level hierarchy, with the given instance descriptors
	D3D12_BUILD_RAYTRACING_ACCELERATION_STRUCTURE_INPUTS Desc	= {};
	Desc.Type													= D3D12_RAYTRACING_ACCELERATION_STRUCTURE_TYPE_TOP_LEVEL;
	Desc.Flags													= D3D12_RAYTRACING_ACCELERATION_STRUCTURE_BUILD_FLAG_PREFER_FAST_BUILD;
	Desc.NumDescs												= Size();
	Desc.DescsLayout											= D3D12_ELEMENTS_LAYOUT_ARRAY;

	// This structure is used to hold the sizes of the required scratch memory and
	// resulting AS
	D3D12_RAYTRACING_ACCELERATION_STRUCTURE_PREBUILD_INFO PrebuildInfo = {};

	// Building the acceleration structure (AS) requires some scratch space, as
	// well as space to store the resulting structure This function computes a
	// conservative estimate of the memory requirements for both, based on the
	// number of bottom-level instances.
	pDevice->GetRaytracingAccelerationStructurePrebuildInfo(&Desc, &PrebuildInfo);

	// Buffer sizes need to be 256-byte-aligned
	ScratchSizeInBytes			= AlignUp<UINT64>(PrebuildInfo.ScratchDataSizeInBytes, D3D12_RAYTRACING_ACCELERATION_STRUCTURE_BYTE_ALIGNMENT);
	ResultSizeInBytes			= AlignUp<UINT64>(PrebuildInfo.ResultDataMaxSizeInBytes, D3D12_RAYTRACING_ACCELERATION_STRUCTURE_BYTE_ALIGNMENT);

	*pScratchSizeInBytes		= ScratchSizeInBytes;
	*pResultSizeInBytes			= ResultSizeInBytes;
}

void TopLevelAccelerationStructure::Generate(ID3D12GraphicsCommandList6* pCommandList, ID3D12Resource* pScratch, ID3D12Resource* pResult, D3D12_GPU_VIRTUAL_ADDRESS InstanceDescs)
{
	if (ScratchSizeInBytes == 0 || ResultSizeInBytes == 0)
	{
		throw std::logic_error("Invalid allocation - ComputeMemoryRequirements needs to be called before Generate");
	}

	if (!pResult || !pScratch)
	{
		throw std::logic_error("Invalid pResult, pScratch, and pInstanceDescs buffers");
	}

	// Create a descriptor of the requested builder work, to generate a top - level
	// AS from the input parameters
	D3D12_BUILD_RAYTRACING_ACCELERATION_STRUCTURE_DESC Desc = {};
	Desc.DestAccelerationStructureData						= pResult->GetGPUVirtualAddress();
	Desc.Inputs.Type										= D3D12_RAYTRACING_ACCELERATION_STRUCTURE_TYPE_TOP_LEVEL;
	Desc.Inputs.Flags										= D3D12_RAYTRACING_ACCELERATION_STRUCTURE_BUILD_FLAG_PREFER_FAST_BUILD;
	Desc.Inputs.NumDescs									= Size();
	Desc.Inputs.DescsLayout									= D3D12_ELEMENTS_LAYOUT_ARRAY;
	Desc.Inputs.InstanceDescs								= InstanceDescs;
	Desc.SourceAccelerationStructureData					= NULL;
	Desc.ScratchAccelerationStructureData					= pScratch->GetGPUVirtualAddress();

	// Build the top-level AS
	pCommandList->BuildRaytracingAccelerationStructure(&Desc, 0, nullptr);
}