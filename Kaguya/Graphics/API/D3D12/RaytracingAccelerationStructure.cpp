#include "pch.h"
#include "RaytracingAccelerationStructure.h"
#include "Buffer.h"
#include "CommandContext.h"

using namespace DirectX;

BottomLevelAccelerationStructure::BottomLevelAccelerationStructure()
{
	ScratchSizeInBytes = ResultSizeInBytes = 0;
	BuildFlags = D3D12_RAYTRACING_ACCELERATION_STRUCTURE_BUILD_FLAG_PREFER_FAST_TRACE;
}

void BottomLevelAccelerationStructure::AddGeometry(const RaytracingGeometryDesc& Desc)
{
	D3D12_RAYTRACING_GEOMETRY_DESC ApiDesc			= {};
	ApiDesc.Type									= D3D12_RAYTRACING_GEOMETRY_TYPE_TRIANGLES;
	ApiDesc.Flags									= Desc.IsOpaque ? D3D12_RAYTRACING_GEOMETRY_FLAG_OPAQUE : D3D12_RAYTRACING_GEOMETRY_FLAG_NONE;
	ApiDesc.Triangles.Transform3x4					= NULL;
	ApiDesc.Triangles.IndexFormat					= DXGI_FORMAT_R32_UINT;
	ApiDesc.Triangles.VertexFormat					= DXGI_FORMAT_R32G32B32_FLOAT; // Position attribute of the vertex
	ApiDesc.Triangles.IndexCount					= Desc.NumIndices;
	ApiDesc.Triangles.VertexCount					= Desc.NumVertices;
	ApiDesc.Triangles.IndexBuffer					= Desc.pIndexBuffer->GetGpuVirtualAddress() + Desc.IndexOffset * Desc.IndexStride;
	ApiDesc.Triangles.VertexBuffer.StartAddress		= Desc.pVertexBuffer->GetGpuVirtualAddress() + Desc.VertexOffset * Desc.VertexStride;
	ApiDesc.Triangles.VertexBuffer.StrideInBytes	= Desc.VertexStride;

	RaytracingGeometryDescs.push_back(ApiDesc);
}

void BottomLevelAccelerationStructure::ComputeMemoryRequirements(ID3D12Device5* pDevice, UINT64* pScratchSizeInBytes, UINT64* pResultSizeInBytes)
{
	D3D12_BUILD_RAYTRACING_ACCELERATION_STRUCTURE_INPUTS Desc	= {};
	Desc.Type													= D3D12_RAYTRACING_ACCELERATION_STRUCTURE_TYPE_BOTTOM_LEVEL;
	Desc.Flags													= BuildFlags;
	Desc.NumDescs												= static_cast<UINT>(RaytracingGeometryDescs.size());
	Desc.DescsLayout											= D3D12_ELEMENTS_LAYOUT_ARRAY;
	Desc.pGeometryDescs											= RaytracingGeometryDescs.data();

	D3D12_RAYTRACING_ACCELERATION_STRUCTURE_PREBUILD_INFO PrebuildInfo = {};
	pDevice->GetRaytracingAccelerationStructurePrebuildInfo(&Desc, &PrebuildInfo);

	// Buffer sizes need to be 256-byte-aligned
	ScratchSizeInBytes		= Math::AlignUp<UINT64>(PrebuildInfo.ScratchDataSizeInBytes, D3D12_RAYTRACING_ACCELERATION_STRUCTURE_BYTE_ALIGNMENT);
	ResultSizeInBytes		= Math::AlignUp<UINT64>(PrebuildInfo.ResultDataMaxSizeInBytes, D3D12_RAYTRACING_ACCELERATION_STRUCTURE_BYTE_ALIGNMENT);

	*pScratchSizeInBytes	= ScratchSizeInBytes;
	*pResultSizeInBytes		= ResultSizeInBytes;
}

void BottomLevelAccelerationStructure::Generate(CommandContext* pCommandContext, Buffer* pScratch, Buffer* pResult)
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
	Desc.DestAccelerationStructureData						= pResult->GetGpuVirtualAddress();
	Desc.Inputs.Type										= D3D12_RAYTRACING_ACCELERATION_STRUCTURE_TYPE_BOTTOM_LEVEL;
	Desc.Inputs.Flags										= BuildFlags;
	Desc.Inputs.NumDescs									= static_cast<UINT>(RaytracingGeometryDescs.size());
	Desc.Inputs.DescsLayout									= D3D12_ELEMENTS_LAYOUT_ARRAY;
	Desc.Inputs.pGeometryDescs								= RaytracingGeometryDescs.data();
	Desc.SourceAccelerationStructureData					= NULL;
	Desc.ScratchAccelerationStructureData					= pScratch->GetGpuVirtualAddress();

	// Build the AS
	pCommandContext->BuildRaytracingAccelerationStructure(&Desc, 0, nullptr);

	// Wait for the builder to complete by setting a barrier on the resulting
	// buffer. This is particularly important as the construction of the top-level
	// hierarchy may be called right afterwards, before executing the command
	// list.
	pCommandContext->UAVBarrier(pResult);
}

TopLevelAccelerationStructure::TopLevelAccelerationStructure()
{
	NumInstances = 0;
	ScratchSizeInBytes = ResultSizeInBytes = 0;
}

void TopLevelAccelerationStructure::SetNumInstances(UINT NumInstances)
{
	this->NumInstances = NumInstances;
}

void TopLevelAccelerationStructure::ComputeMemoryRequirements(ID3D12Device5* pDevice, UINT64* pScratchSizeInBytes, UINT64* pResultSizeInBytes)
{
	// Describe the work being requested, in this case the construction of a
	// (possibly dynamic) top-level hierarchy, with the given instance descriptors
	D3D12_BUILD_RAYTRACING_ACCELERATION_STRUCTURE_INPUTS Desc	= {};
	Desc.Type													= D3D12_RAYTRACING_ACCELERATION_STRUCTURE_TYPE_TOP_LEVEL;
	Desc.Flags													= D3D12_RAYTRACING_ACCELERATION_STRUCTURE_BUILD_FLAG_PREFER_FAST_BUILD;
	Desc.NumDescs												= NumInstances;
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
	ScratchSizeInBytes			= Math::AlignUp<UINT64>(PrebuildInfo.ScratchDataSizeInBytes, D3D12_RAYTRACING_ACCELERATION_STRUCTURE_BYTE_ALIGNMENT);
	ResultSizeInBytes			= Math::AlignUp<UINT64>(PrebuildInfo.ResultDataMaxSizeInBytes, D3D12_RAYTRACING_ACCELERATION_STRUCTURE_BYTE_ALIGNMENT);

	*pScratchSizeInBytes		= ScratchSizeInBytes;
	*pResultSizeInBytes			= ResultSizeInBytes;
}

void TopLevelAccelerationStructure::Generate(CommandContext* pCommandContext, Buffer* pScratch, Buffer* pResult, Buffer* pInstanceDescs)
{
	if (ScratchSizeInBytes == 0 || ResultSizeInBytes == 0)
	{
		throw std::logic_error("Invalid allocation - ComputeMemoryRequirements needs to be called before Generate");
	}

	if (!pResult || !pScratch || !pInstanceDescs)
	{
		throw std::logic_error("Invalid pResult, pScratch, and pInstanceDescs buffers");
	}

	// Create a descriptor of the requested builder work, to generate a top - level
	// AS from the input parameters
	D3D12_BUILD_RAYTRACING_ACCELERATION_STRUCTURE_DESC Desc = {};
	Desc.DestAccelerationStructureData						= pResult->GetGpuVirtualAddress();
	Desc.Inputs.Type										= D3D12_RAYTRACING_ACCELERATION_STRUCTURE_TYPE_TOP_LEVEL;
	Desc.Inputs.Flags										= D3D12_RAYTRACING_ACCELERATION_STRUCTURE_BUILD_FLAG_PREFER_FAST_BUILD;
	Desc.Inputs.NumDescs									= NumInstances;
	Desc.Inputs.DescsLayout									= D3D12_ELEMENTS_LAYOUT_ARRAY;
	Desc.Inputs.InstanceDescs								= pInstanceDescs->GetGpuVirtualAddress();
	Desc.SourceAccelerationStructureData					= NULL;
	Desc.ScratchAccelerationStructureData					= pScratch->GetGpuVirtualAddress();

	// Build the top-level AS
	pCommandContext->BuildRaytracingAccelerationStructure(&Desc, 0, nullptr);

	// Wait for the builder to complete by setting a barrier on the resulting
	// buffer. This can be important in case the rendering is triggered
	// immediately afterwards, without executing the command list
	pCommandContext->UAVBarrier(pResult);
}