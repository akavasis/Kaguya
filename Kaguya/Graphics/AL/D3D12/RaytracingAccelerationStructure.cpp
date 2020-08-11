#include "pch.h"
#include "RaytracingAccelerationStructure.h"
#include "Device.h"
#include "Buffer.h"
#include "CommandContext.h"

BottomLevelAccelerationStructure::BottomLevelAccelerationStructure()
{
	ScratchSizeInBytes = ResultSizeInBytes = 0;
	BuildFlags = D3D12_RAYTRACING_ACCELERATION_STRUCTURE_BUILD_FLAG_NONE;
}

void BottomLevelAccelerationStructure::AddGeometry(const RaytracingGeometryDesc& Desc)
{
	D3D12_RAYTRACING_GEOMETRY_DESC desc = {};
	desc.Type = D3D12_RAYTRACING_GEOMETRY_TYPE_TRIANGLES;
	desc.Flags = Desc.IsOpaque ? D3D12_RAYTRACING_GEOMETRY_FLAG_OPAQUE : D3D12_RAYTRACING_GEOMETRY_FLAG_NONE;
	desc.Triangles.Transform3x4 = NULL;
	desc.Triangles.IndexFormat = DXGI_FORMAT_R32_UINT;
	desc.Triangles.VertexFormat = DXGI_FORMAT_R32G32B32_FLOAT; // Position attribute of the vertex
	desc.Triangles.IndexCount = Desc.NumIndices;
	desc.Triangles.VertexCount = Desc.NumVertices;
	desc.Triangles.IndexBuffer = Desc.pIndexBuffer->GetGpuVirtualAddress() + Desc.IndexOffset * Desc.IndexStride;
	desc.Triangles.VertexBuffer.StartAddress = Desc.pVertexBuffer->GetGpuVirtualAddress() + Desc.VertexOffset * Desc.VertexStride;
	desc.Triangles.VertexBuffer.StrideInBytes = Desc.VertexStride;

	RaytracingGeometryDescs.push_back(desc);
}

void BottomLevelAccelerationStructure::ComputeMemoryRequirements(const Device* pDevice, bool AllowUpdate, UINT64* pScratchSizeInBytes, UINT64* pResultSizeInBytes)
{
	assert(pScratchSizeInBytes != nullptr);
	assert(pResultSizeInBytes != nullptr);

	// The generated AS can support iterative updates. This may change the final
	// size of the AS as well as the temporary memory requirements, and hence has
	// to be set before the actual build
	BuildFlags = AllowUpdate ? D3D12_RAYTRACING_ACCELERATION_STRUCTURE_BUILD_FLAG_ALLOW_UPDATE : D3D12_RAYTRACING_ACCELERATION_STRUCTURE_BUILD_FLAG_NONE;

	// Describe the work being requested, in this case the construction of a
	// (possibly dynamic) bottom-level hierarchy, with the given vertex buffers
	D3D12_BUILD_RAYTRACING_ACCELERATION_STRUCTURE_INPUTS desc = {};
	desc.Type = D3D12_RAYTRACING_ACCELERATION_STRUCTURE_TYPE_BOTTOM_LEVEL;
	desc.Flags = BuildFlags;
	desc.NumDescs = static_cast<UINT>(RaytracingGeometryDescs.size());
	desc.DescsLayout = D3D12_ELEMENTS_LAYOUT_ARRAY;
	desc.pGeometryDescs = RaytracingGeometryDescs.data();

	// This structure is used to hold the sizes of the required scratch memory and
	// resulting AS
	D3D12_RAYTRACING_ACCELERATION_STRUCTURE_PREBUILD_INFO info = {};
	pDevice->GetD3DDevice()->GetRaytracingAccelerationStructurePrebuildInfo(&desc, &info);

	// Buffer sizes need to be 256-byte-aligned
	ScratchSizeInBytes = Math::AlignUp<UINT64>(info.ScratchDataSizeInBytes, D3D12_RAYTRACING_ACCELERATION_STRUCTURE_BYTE_ALIGNMENT);
	ResultSizeInBytes = Math::AlignUp<UINT64>(info.ResultDataMaxSizeInBytes, D3D12_RAYTRACING_ACCELERATION_STRUCTURE_BYTE_ALIGNMENT);

	*pScratchSizeInBytes = ScratchSizeInBytes;
	*pResultSizeInBytes = ResultSizeInBytes;
}

void BottomLevelAccelerationStructure::Generate(CommandContext* pCommandContext, Buffer* pScratch, Buffer* pResult, bool UpdateOnly /*= false*/, Buffer* pSource /*= nullptr*/)
{
	D3D12_RAYTRACING_ACCELERATION_STRUCTURE_BUILD_FLAGS buildFlags = BuildFlags;

	// The stored flags represent whether the AS has been built for updates or
	// not. If yes and an update is requested, the builder is told to only update
	// the AS instead of fully rebuilding it
	if (buildFlags == D3D12_RAYTRACING_ACCELERATION_STRUCTURE_BUILD_FLAG_ALLOW_UPDATE && UpdateOnly)
	{
		buildFlags = D3D12_RAYTRACING_ACCELERATION_STRUCTURE_BUILD_FLAG_PERFORM_UPDATE;
	}

	// Sanity checks
	if (BuildFlags != D3D12_RAYTRACING_ACCELERATION_STRUCTURE_BUILD_FLAG_ALLOW_UPDATE && UpdateOnly)
	{
		throw std::logic_error("Cannot update a bottom-level acceleration structure not originally built for updates");
	}

	if (UpdateOnly && !pSource)
	{
		throw std::logic_error("Bottom-level acceleration structure update requires the source acceleration structure");
	}

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
	D3D12_BUILD_RAYTRACING_ACCELERATION_STRUCTURE_DESC desc = {};
	desc.DestAccelerationStructureData = pResult->GetGpuVirtualAddress();
	desc.Inputs.Type = D3D12_RAYTRACING_ACCELERATION_STRUCTURE_TYPE_BOTTOM_LEVEL;
	desc.Inputs.Flags = buildFlags;
	desc.Inputs.NumDescs = static_cast<UINT>(RaytracingGeometryDescs.size());
	desc.Inputs.DescsLayout = D3D12_ELEMENTS_LAYOUT_ARRAY;
	desc.Inputs.pGeometryDescs = RaytracingGeometryDescs.data();
	desc.SourceAccelerationStructureData = pSource ? pSource->GetGpuVirtualAddress() : NULL;
	desc.ScratchAccelerationStructureData = pScratch->GetGpuVirtualAddress();

	// Build the AS
	pCommandContext->BuildRaytracingAccelerationStructure(&desc, 0, nullptr);

	// Wait for the builder to complete by setting a barrier on the resulting
	// buffer. This is particularly important as the construction of the top-level
	// hierarchy may be called right afterwards, before executing the command
	// list.
	pCommandContext->UAVBarrier(pResult);
	pCommandContext->FlushResourceBarriers();
}

TopLevelAccelerationStructure::TopLevelAccelerationStructure()
{
	BuildFlags = D3D12_RAYTRACING_ACCELERATION_STRUCTURE_BUILD_FLAG_NONE;
	ScratchSizeInBytes = ResultSizeInBytes = InstanceDescsSizeInBytes = 0;
}

void TopLevelAccelerationStructure::AddInstance(const RaytracingInstanceDesc& Desc)
{
	D3D12_RAYTRACING_INSTANCE_DESC desc = {};
	memcpy(desc.Transform, &DirectX::XMMatrixTranspose(Desc.Transform), sizeof(desc.Transform));
	desc.InstanceID = Desc.InstanceID;
	desc.InstanceMask = 0xFF; // TODO: should be accessible from outside
	desc.InstanceContributionToHitGroupIndex = Desc.HitGroupIndex;
	desc.Flags = D3D12_RAYTRACING_INSTANCE_FLAG_NONE; // TODO: should be accessible from outside
	desc.AccelerationStructure = Desc.pBottomLevelAccelerationStructure->GetGpuVirtualAddress();

	RaytracingInstanceDescs.push_back(desc);
}

void TopLevelAccelerationStructure::ComputeMemoryRequirements(const Device* pDevice, bool AllowUpdate, UINT64* pScratchSizeInBytes, UINT64* pResultSizeInBytes, UINT64* pInstanceDescsSizeInBytes)
{
	// The generated AS can support iterative updates. This may change the final
	// size of the AS as well as the temporary memory requirements, and hence has
	// to be set before the actual build
	BuildFlags = AllowUpdate ? D3D12_RAYTRACING_ACCELERATION_STRUCTURE_BUILD_FLAG_ALLOW_UPDATE : D3D12_RAYTRACING_ACCELERATION_STRUCTURE_BUILD_FLAG_NONE;

	// Describe the work being requested, in this case the construction of a
	// (possibly dynamic) top-level hierarchy, with the given instance descriptors
	D3D12_BUILD_RAYTRACING_ACCELERATION_STRUCTURE_INPUTS desc = {};
	desc.Type = D3D12_RAYTRACING_ACCELERATION_STRUCTURE_TYPE_TOP_LEVEL;
	desc.Flags = BuildFlags;
	desc.NumDescs = static_cast<UINT>(RaytracingInstanceDescs.size());
	desc.DescsLayout = D3D12_ELEMENTS_LAYOUT_ARRAY;

	// This structure is used to hold the sizes of the required scratch memory and
	// resulting AS
	D3D12_RAYTRACING_ACCELERATION_STRUCTURE_PREBUILD_INFO info = {};

	// Building the acceleration structure (AS) requires some scratch space, as
	// well as space to store the resulting structure This function computes a
	// conservative estimate of the memory requirements for both, based on the
	// number of bottom-level instances.
	pDevice->GetD3DDevice()->GetRaytracingAccelerationStructurePrebuildInfo(&desc, &info);

	// Buffer sizes need to be 256-byte-aligned
	ScratchSizeInBytes = Math::AlignUp<UINT64>(info.ScratchDataSizeInBytes, D3D12_RAYTRACING_ACCELERATION_STRUCTURE_BYTE_ALIGNMENT);
	ResultSizeInBytes = Math::AlignUp<UINT64>(info.ResultDataMaxSizeInBytes, D3D12_RAYTRACING_ACCELERATION_STRUCTURE_BYTE_ALIGNMENT);
	InstanceDescsSizeInBytes = Math::AlignUp<UINT64>(sizeof(D3D12_RAYTRACING_INSTANCE_DESC) * static_cast<UINT64>(RaytracingInstanceDescs.size()), D3D12_CONSTANT_BUFFER_DATA_PLACEMENT_ALIGNMENT);

	*pScratchSizeInBytes = ScratchSizeInBytes;
	*pResultSizeInBytes = ResultSizeInBytes;
	*pInstanceDescsSizeInBytes = InstanceDescsSizeInBytes;
}

void TopLevelAccelerationStructure::Generate(CommandContext* pCommandContext, Buffer* pScratch, Buffer* pResult, Buffer* pInstanceDescs, bool UpdateOnly /*= false*/, Buffer* pSource /*= nullptr*/)
{
	D3D12_RAYTRACING_INSTANCE_DESC* pInstanceDesc = nullptr;
	pInstanceDescs->GetD3DResource()->Map(0, nullptr, reinterpret_cast<void**>(&pInstanceDesc));
	if (!pInstanceDesc)
	{
		throw std::logic_error("Cannot map the instance descriptor buffer - is it in the upload heap?");
	}

	UINT numInstances = static_cast<UINT>(RaytracingInstanceDescs.size());

	// Initialize the memory to zero on the first time only
	if (!UpdateOnly)
	{
		ZeroMemory(pInstanceDesc, InstanceDescsSizeInBytes);
	}

	// Create the description for each instance
	for (UINT i = 0; i < numInstances; i++)
	{
		pInstanceDesc[i] = RaytracingInstanceDescs[i];
	}

	pInstanceDescs->GetD3DResource()->Unmap(0, nullptr);

	D3D12_RAYTRACING_ACCELERATION_STRUCTURE_BUILD_FLAGS buildFlags = BuildFlags;

	// The stored flags represent whether the AS has been built for updates or
	// not. If yes and an update is requested, the builder is told to only update
	// the AS instead of fully rebuilding it
	if (buildFlags == D3D12_RAYTRACING_ACCELERATION_STRUCTURE_BUILD_FLAG_ALLOW_UPDATE && UpdateOnly)
	{
		buildFlags = D3D12_RAYTRACING_ACCELERATION_STRUCTURE_BUILD_FLAG_PERFORM_UPDATE;
	}

	// Sanity checks
	if (BuildFlags != D3D12_RAYTRACING_ACCELERATION_STRUCTURE_BUILD_FLAG_ALLOW_UPDATE && UpdateOnly)
	{
		throw std::logic_error("Cannot update a top-level acceleration structure not originally built for updates");
	}

	if (UpdateOnly && !pSource)
	{
		throw std::logic_error("Top-level acceleration structure update requires the source acceleration structure");
	}

	if (ScratchSizeInBytes == 0 || ResultSizeInBytes == 0 || InstanceDescsSizeInBytes == 0)
	{
		throw std::logic_error("Invalid Result, Scratch and InstanceDescs buffer sizes - ComputeMemoryRequirements needs to be called before Build");
	}

	if (!pResult || !pScratch || !pInstanceDescs)
	{
		throw std::logic_error("Invalid pResult, pScratch, and pInstanceDescs buffers");
	}

	// Create a descriptor of the requested builder work, to generate a top - level
	// AS from the input parameters
	D3D12_BUILD_RAYTRACING_ACCELERATION_STRUCTURE_DESC desc = {};
	desc.DestAccelerationStructureData = pResult->GetGpuVirtualAddress();
	desc.Inputs.Type = D3D12_RAYTRACING_ACCELERATION_STRUCTURE_TYPE_TOP_LEVEL;
	desc.Inputs.Flags = buildFlags;
	desc.Inputs.NumDescs = numInstances;
	desc.Inputs.DescsLayout = D3D12_ELEMENTS_LAYOUT_ARRAY;
	desc.Inputs.InstanceDescs = pInstanceDescs->GetGpuVirtualAddress();
	desc.SourceAccelerationStructureData = pSource ? pSource->GetGpuVirtualAddress() : NULL;
	desc.ScratchAccelerationStructureData = pScratch->GetGpuVirtualAddress();

	// Build the top-level AS
	pCommandContext->BuildRaytracingAccelerationStructure(&desc, 0, nullptr);

	// Wait for the builder to complete by setting a barrier on the resulting
	// buffer. This can be important in case the rendering is triggered
	// immediately afterwards, without executing the command list
	pCommandContext->UAVBarrier(pResult);
	pCommandContext->FlushResourceBarriers();
}