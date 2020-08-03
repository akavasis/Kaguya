#pragma once
#include "Device.h"
#include "CommandContext.h"

#include "Buffer.h"

struct RaytracingAccelerationStructure
{
	Buffer* pScratch;
	Buffer* pResult;
	Buffer* pInstanceDesc;
};

class BLASGenerator
{
public:
	void AddBuffer(Buffer* pVertexBuffer,
		UINT VertexCount,
		UINT64 VertexStride,
		Buffer* pIndexBuffer,
		UINT IndexCount,
		bool IsOpaque)
	{
		D3D12_RAYTRACING_GEOMETRY_DESC desc = {};
		desc.Type = D3D12_RAYTRACING_GEOMETRY_TYPE_TRIANGLES;
		desc.Flags = IsOpaque ? D3D12_RAYTRACING_GEOMETRY_FLAG_OPAQUE : D3D12_RAYTRACING_GEOMETRY_FLAG_NONE;
		desc.Triangles.Transform3x4 = NULL;
		desc.Triangles.IndexFormat = DXGI_FORMAT_R32_UINT;
		desc.Triangles.VertexFormat = DXGI_FORMAT_R32G32B32_FLOAT; // Position attribute of the vertex
		desc.Triangles.IndexCount = IndexCount;
		desc.Triangles.VertexCount = VertexCount;
		desc.Triangles.IndexBuffer = pIndexBuffer->GetGpuVirtualAddress();
		desc.Triangles.VertexBuffer.StartAddress = pVertexBuffer->GetGpuVirtualAddress();
		desc.Triangles.VertexBuffer.StrideInBytes = VertexStride;

		m_RTGeometryDescs.push_back(desc);
	}

	void ComputeASBufferMemoryRequirements(Device* pDevice, bool AllowUpdate, UINT64* pScratchSizeInBytes, UINT64* pResultSizeInBytes)
	{
		// The generated AS can support iterative updates. This may change the final
		// size of the AS as well as the temporary memory requirements, and hence has
		// to be set before the actual build
		m_ASBuildFlags = AllowUpdate ? D3D12_RAYTRACING_ACCELERATION_STRUCTURE_BUILD_FLAG_ALLOW_UPDATE : D3D12_RAYTRACING_ACCELERATION_STRUCTURE_BUILD_FLAG_NONE;

		// Describe the work being requested, in this case the construction of a
		// (possibly dynamic) bottom-level hierarchy, with the given vertex buffers
		D3D12_BUILD_RAYTRACING_ACCELERATION_STRUCTURE_INPUTS prebuildDesc = {};
		prebuildDesc.Type = D3D12_RAYTRACING_ACCELERATION_STRUCTURE_TYPE_BOTTOM_LEVEL;
		prebuildDesc.Flags = m_ASBuildFlags;
		prebuildDesc.NumDescs = static_cast<UINT>(m_RTGeometryDescs.size());
		prebuildDesc.DescsLayout = D3D12_ELEMENTS_LAYOUT_ARRAY;
		prebuildDesc.pGeometryDescs = m_RTGeometryDescs.data();

		// This structure is used to hold the sizes of the required scratch memory and
		// resulting AS
		D3D12_RAYTRACING_ACCELERATION_STRUCTURE_PREBUILD_INFO prebuildInfo = {};
		pDevice->GetD3DDevice()->GetRaytracingAccelerationStructurePrebuildInfo(&prebuildDesc, &prebuildInfo);

		// Buffer sizes need to be 256-byte-aligned
		*pScratchSizeInBytes = Math::AlignUp<UINT64>(prebuildInfo.ScratchDataSizeInBytes, D3D12_RAYTRACING_ACCELERATION_STRUCTURE_BYTE_ALIGNMENT);
		*pResultSizeInBytes = Math::AlignUp<UINT64>(prebuildInfo.ResultDataMaxSizeInBytes, D3D12_RAYTRACING_ACCELERATION_STRUCTURE_BYTE_ALIGNMENT);

		// Store the memory requirements for use during build
		m_ScratchSizeInBytes = *pScratchSizeInBytes;
		m_ResultSizeInBytes = *pResultSizeInBytes;
	}

	void Generate(CommandContext* pCommandContext, Buffer* pScratch, Buffer* pResult, bool UpdateOnly, Buffer* pPreviousResult)
	{
		D3D12_RAYTRACING_ACCELERATION_STRUCTURE_BUILD_FLAGS buildFlags = m_ASBuildFlags;

		if (buildFlags == D3D12_RAYTRACING_ACCELERATION_STRUCTURE_BUILD_FLAG_ALLOW_UPDATE &&
			UpdateOnly)
		{
			buildFlags = D3D12_RAYTRACING_ACCELERATION_STRUCTURE_BUILD_FLAG_PERFORM_UPDATE;
		}

		// Sanity checks
		if (m_ASBuildFlags !=
			D3D12_RAYTRACING_ACCELERATION_STRUCTURE_BUILD_FLAG_ALLOW_UPDATE &&
			UpdateOnly)
		{
			throw std::logic_error(
				"Cannot update a bottom-level AS not originally built for updates");
		}
		if (UpdateOnly && !pPreviousResult)
		{
			throw std::logic_error(
				"Bottom-level hierarchy update requires the previous hierarchy");
		}

		if (m_ResultSizeInBytes == 0 || m_ScratchSizeInBytes == 0)
		{
			throw std::logic_error(
				"Invalid scratch and result buffer sizes - ComputeASBufferSizes needs "
				"to be called before Build");
		}

		// Create a descriptor of the requested builder work, to generate a
		// bottom-level AS from the input parameters
		D3D12_BUILD_RAYTRACING_ACCELERATION_STRUCTURE_DESC desc;
		desc.DestAccelerationStructureData = pResult->GetGpuVirtualAddress();
		desc.Inputs.Type = D3D12_RAYTRACING_ACCELERATION_STRUCTURE_TYPE_BOTTOM_LEVEL;
		desc.Inputs.Flags = buildFlags;
		desc.Inputs.NumDescs = static_cast<UINT>(m_RTGeometryDescs.size());
		desc.Inputs.DescsLayout = D3D12_ELEMENTS_LAYOUT_ARRAY;
		desc.Inputs.pGeometryDescs = m_RTGeometryDescs.data();

		desc.SourceAccelerationStructureData = pPreviousResult ? pPreviousResult->GetGpuVirtualAddress() : 0;
		desc.ScratchAccelerationStructureData = pScratch->GetGpuVirtualAddress();

		// Build the AS
		pCommandContext->BuildRaytracingAccelerationStructure(&desc, 0, nullptr);

		// Wait for the builder to complete by setting a barrier on the resulting
		// buffer. This is particularly important as the construction of the top-level
		// hierarchy may be called right afterwards, before executing the command
		// list.
		pCommandContext->UAVBarrier(pResult);
	}
private:
	// Vertex buffer descriptors used to generate the AS
	std::vector<D3D12_RAYTRACING_GEOMETRY_DESC> m_RTGeometryDescs;

	// Amount of temporary memory required by the builder
	UINT64 m_ScratchSizeInBytes;

	// Amount of memory required to store the AS
	UINT64 m_ResultSizeInBytes;

	// Flags for the builder, specifying whether to allow iterative updates, or
	// when to perform an update
	D3D12_RAYTRACING_ACCELERATION_STRUCTURE_BUILD_FLAGS m_ASBuildFlags;
};