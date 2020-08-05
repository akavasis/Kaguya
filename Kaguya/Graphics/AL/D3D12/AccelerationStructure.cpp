#include "pch.h"
#include "AccelerationStructure.h"

RaytracingAccelerationStructure::RaytracingAccelerationStructure(const Device* pDevice)
	: pDevice(pDevice)
{
	pResult = pScratch = pSource = nullptr;
	m_ASDesc = {};
	m_ASBuildFlags = {};
}
void RaytracingAccelerationStructure::SetAccelerationStructure(Buffer* pResult, Buffer* pScratch, Buffer* pSource)
{
	assert(pResult);
	assert(pScratch);

	this->pResult = pResult;
	this->pScratch = pScratch;
	this->pSource = pSource;

	m_ASDesc.DestAccelerationStructureData = this->pResult->GetGpuVirtualAddress();
	m_ASDesc.ScratchAccelerationStructureData = this->pScratch->GetGpuVirtualAddress();
	m_ASDesc.SourceAccelerationStructureData = this->pSource ? this->pSource->GetGpuVirtualAddress() : NULL;
}

BottomLevelAccelerationStructure::BottomLevelAccelerationStructure(const Device* pDevice)
	: RaytracingAccelerationStructure(pDevice)
{
	m_ASBuildFlags = D3D12_RAYTRACING_ACCELERATION_STRUCTURE_BUILD_FLAG_NONE;
	m_ASDesc.Inputs.Type = D3D12_RAYTRACING_ACCELERATION_STRUCTURE_TYPE_BOTTOM_LEVEL;
	m_ASDesc.Inputs.DescsLayout = D3D12_ELEMENTS_LAYOUT_ARRAY;
	m_MemoryRequirements = {};
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

	m_RTGeometryDescs.push_back(desc);
}

BottomLevelAccelerationStructure::MemoryRequirements BottomLevelAccelerationStructure::GetAccelerationStructureMemoryRequirements(bool AllowUpdate)
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
	m_MemoryRequirements.ResultDataMaxSizeInBytes = Math::AlignUp<UINT64>(prebuildInfo.ResultDataMaxSizeInBytes, D3D12_RAYTRACING_ACCELERATION_STRUCTURE_BYTE_ALIGNMENT);
	m_MemoryRequirements.ScratchDataSizeInBytes = Math::AlignUp<UINT64>(prebuildInfo.ScratchDataSizeInBytes, D3D12_RAYTRACING_ACCELERATION_STRUCTURE_BYTE_ALIGNMENT);
	m_MemoryRequirements.UpdateScratchDataSizeInBytes = Math::AlignUp<UINT64>(prebuildInfo.UpdateScratchDataSizeInBytes, D3D12_RAYTRACING_ACCELERATION_STRUCTURE_BYTE_ALIGNMENT);

	return m_MemoryRequirements;
}

void BottomLevelAccelerationStructure::Generate(bool UpdateOnly, CommandContext* pCommandContext)
{
	D3D12_RAYTRACING_ACCELERATION_STRUCTURE_BUILD_FLAGS buildFlags = m_ASBuildFlags;
	if (buildFlags == D3D12_RAYTRACING_ACCELERATION_STRUCTURE_BUILD_FLAG_ALLOW_UPDATE &&
		UpdateOnly)
	{
		buildFlags = D3D12_RAYTRACING_ACCELERATION_STRUCTURE_BUILD_FLAG_PERFORM_UPDATE;
	}

	// Sanity checks
	if (m_ASBuildFlags != D3D12_RAYTRACING_ACCELERATION_STRUCTURE_BUILD_FLAG_ALLOW_UPDATE && UpdateOnly)
	{
		throw std::logic_error("Cannot update a bottom-level AS not originally built for updates");
	}

	if (UpdateOnly && !pSource)
	{
		throw std::logic_error("Bottom-level hierarchy update requires the previous hierarchy");
	}

	if (m_MemoryRequirements.ResultDataMaxSizeInBytes == 0 ||
		m_MemoryRequirements.ScratchDataSizeInBytes == 0)
	{
		throw std::logic_error("Invalid Result and Scratch buffer sizes - GetAccelerationStructureMemoryRequirements needs to be called before Build");
	}

	if (!pResult || !pScratch)
	{
		throw std::logic_error("Invalid pResult and pScratch buffers - SetAccelerationStructure needs to be called before Build");
	}

	// Create a descriptor of the requested builder work, to generate a
	// bottom-level AS from the input parameters
	m_ASDesc.Inputs.Flags = buildFlags;
	m_ASDesc.Inputs.NumDescs = static_cast<UINT>(m_RTGeometryDescs.size());
	m_ASDesc.Inputs.pGeometryDescs = m_RTGeometryDescs.data();

	// Build the AS
	pCommandContext->BuildRaytracingAccelerationStructure(&m_ASDesc, 0, nullptr);

	// Wait for the builder to complete by setting a barrier on the resulting
	// buffer. This is particularly important as the construction of the top-level
	// hierarchy may be called right afterwards, before executing the command
	// list.
	pCommandContext->UAVBarrier(pResult);
}

TopLevelAccelerationStructure::TopLevelAccelerationStructure(const Device* pDevice)
	: RaytracingAccelerationStructure(pDevice)
{
	m_ASBuildFlags = D3D12_RAYTRACING_ACCELERATION_STRUCTURE_BUILD_FLAG_NONE;
	m_ASDesc.Inputs.Type = D3D12_RAYTRACING_ACCELERATION_STRUCTURE_TYPE_TOP_LEVEL;
	m_ASDesc.Inputs.DescsLayout = D3D12_ELEMENTS_LAYOUT_ARRAY;
	m_MemoryRequirements = {};
}

void TopLevelAccelerationStructure::SetInstanceBuffer(Buffer* pInstance)
{
	m_pInstance = pInstance;
}

void TopLevelAccelerationStructure::AddInstance(Buffer* pBLAS, Transform Transform, UINT InstanceID, UINT HitGroupIndex)
{
	D3D12_RAYTRACING_INSTANCE_DESC desc;
	memcpy(desc.Transform, &DirectX::XMMatrixTranspose(Transform.Matrix()), sizeof(desc.Transform));
	desc.InstanceID = InstanceID;
	desc.InstanceMask = 0xFF;
	desc.InstanceContributionToHitGroupIndex = HitGroupIndex;
	desc.Flags = D3D12_RAYTRACING_INSTANCE_FLAG_NONE;
	desc.AccelerationStructure = pBLAS->GetGpuVirtualAddress();
	m_RTInstanceDescs.push_back(desc);
}

TopLevelAccelerationStructure::MemoryRequirements TopLevelAccelerationStructure::GetAccelerationStructureMemoryRequirements(bool AllowUpdate)
{
	// The generated AS can support iterative updates. This may change the final
	// size of the AS as well as the temporary memory requirements, and hence has
	// to be set before the actual build
	m_ASBuildFlags = AllowUpdate ? D3D12_RAYTRACING_ACCELERATION_STRUCTURE_BUILD_FLAG_ALLOW_UPDATE : D3D12_RAYTRACING_ACCELERATION_STRUCTURE_BUILD_FLAG_NONE;

	// Describe the work being requested, in this case the construction of a
	// (possibly dynamic) top-level hierarchy, with the given instance descriptors
	D3D12_BUILD_RAYTRACING_ACCELERATION_STRUCTURE_INPUTS prebuildDesc = {};
	prebuildDesc.Type = D3D12_RAYTRACING_ACCELERATION_STRUCTURE_TYPE_TOP_LEVEL;
	prebuildDesc.Flags = m_ASBuildFlags;
	prebuildDesc.NumDescs = static_cast<UINT>(m_RTInstanceDescs.size());
	prebuildDesc.DescsLayout = D3D12_ELEMENTS_LAYOUT_ARRAY;

	// This structure is used to hold the sizes of the required scratch memory and
	// resulting AS
	D3D12_RAYTRACING_ACCELERATION_STRUCTURE_PREBUILD_INFO prebuildInfo = {};

	// Building the acceleration structure (AS) requires some scratch space, as
	// well as space to store the resulting structure This function computes a
	// conservative estimate of the memory requirements for both, based on the
	// number of bottom-level instances.
	pDevice->GetD3DDevice()->GetRaytracingAccelerationStructurePrebuildInfo(&prebuildDesc, &prebuildInfo);

	// Buffer sizes need to be 256-byte-aligned
	m_MemoryRequirements.ResultDataMaxSizeInBytes = Math::AlignUp<UINT64>(prebuildInfo.ResultDataMaxSizeInBytes, D3D12_RAYTRACING_ACCELERATION_STRUCTURE_BYTE_ALIGNMENT);
	m_MemoryRequirements.ScratchDataSizeInBytes = Math::AlignUp<UINT64>(prebuildInfo.ScratchDataSizeInBytes, D3D12_RAYTRACING_ACCELERATION_STRUCTURE_BYTE_ALIGNMENT);
	m_MemoryRequirements.UpdateScratchDataSizeInBytes = Math::AlignUp<UINT64>(prebuildInfo.UpdateScratchDataSizeInBytes, D3D12_RAYTRACING_ACCELERATION_STRUCTURE_BYTE_ALIGNMENT);
	m_MemoryRequirements.InstanceDataSizeInBytes = Math::AlignUp<UINT64>(sizeof(D3D12_RAYTRACING_INSTANCE_DESC) * static_cast<UINT64>(m_RTInstanceDescs.size()), D3D12_CONSTANT_BUFFER_DATA_PLACEMENT_ALIGNMENT);

	return m_MemoryRequirements;
}

void TopLevelAccelerationStructure::Generate(bool UpdateOnly, CommandContext* pCommandContext)
{
	auto pGPU = m_pInstance->Map();
	if (!pGPU)
	{
		throw std::logic_error("Cannot map the instance buffer - is it in the upload heap?");
	}

	if (!UpdateOnly)
	{
		ZeroMemory(pGPU, m_MemoryRequirements.InstanceDataSizeInBytes);
	}

	memcpy(pGPU, m_RTInstanceDescs.data(), m_MemoryRequirements.InstanceDataSizeInBytes);

	D3D12_RAYTRACING_ACCELERATION_STRUCTURE_BUILD_FLAGS buildFlags = m_ASBuildFlags;
	if (buildFlags == D3D12_RAYTRACING_ACCELERATION_STRUCTURE_BUILD_FLAG_ALLOW_UPDATE &&
		UpdateOnly)
	{
		buildFlags = D3D12_RAYTRACING_ACCELERATION_STRUCTURE_BUILD_FLAG_PERFORM_UPDATE;
	}

	// Sanity checks
	if (m_ASBuildFlags != D3D12_RAYTRACING_ACCELERATION_STRUCTURE_BUILD_FLAG_ALLOW_UPDATE && UpdateOnly)
	{
		throw std::logic_error("Cannot update a top-level AS not originally built for updates");
	}

	if (UpdateOnly && !pSource)
	{
		throw std::logic_error("Top-level hierarchy update requires the previous hierarchy");
	}

	// Create a descriptor of the requested builder work, to generate a top-level
	// AS from the input parameters
	m_ASDesc.Inputs.Flags = buildFlags;
	m_ASDesc.Inputs.NumDescs = static_cast<UINT>(m_RTInstanceDescs.size());
	m_ASDesc.Inputs.InstanceDescs = m_pInstance->GetGpuVirtualAddress();

	// Build the top-level AS
	pCommandContext->BuildRaytracingAccelerationStructure(&m_ASDesc, 0, nullptr);

	// Wait for the builder to complete by setting a barrier on the resulting
	// buffer. This can be important in case the rendering is triggered
	// immediately afterwards, without executing the command list
	pCommandContext->UAVBarrier(pResult);
}