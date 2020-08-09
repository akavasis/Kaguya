#include "pch.h"
#include "ShaderTable.h"
#include "Buffer.h"
#include "RaytracingPipelineState.h"

ShaderTable::ShaderTable()
{
	m_ShaderRecordStrides.fill(0);
}

void ShaderTable::AddShaderRecord(ShaderTable::RecordType Type, LPCWSTR pExportName, std::vector<void*> RootArguments)
{
	constexpr UINT64 ShaderIdentiferSizeInBytes = sizeof(ShaderIdentifier);
	UINT64 recordSizeInBytes = ShaderIdentiferSizeInBytes;
	recordSizeInBytes += 8 * static_cast<UINT64>(RootArguments.size());
	recordSizeInBytes = Math::AlignUp<UINT64>(recordSizeInBytes, D3D12_RAYTRACING_SHADER_RECORD_BYTE_ALIGNMENT);

	ShaderRecord shaderRecord = {};
	shaderRecord.ExportName = pExportName ? pExportName : L"";
	shaderRecord.RootArguments = std::move(RootArguments);
	shaderRecord.SizeInBytes = recordSizeInBytes;

	m_ShaderRecords[Type].push_back(shaderRecord);
	m_ShaderRecordStrides[Type] = std::max(m_ShaderRecordStrides[Type], recordSizeInBytes);
}

void ShaderTable::Reset()
{
	for (size_t i = 0; i < NumRecordTypes; ++i)
	{
		m_ShaderRecords[i].clear();
		m_ShaderRecordStrides[i] = 0;
	}
}

ShaderTable::MemoryRequirements ShaderTable::GetShaderTableMemoryRequirements() const
{
	UINT64 shaderTableSizeInBytes = 0;
	for (size_t i = 0; i < NumRecordTypes; ++i)
	{
		shaderTableSizeInBytes += static_cast<UINT64>(m_ShaderRecords[i].size()) * m_ShaderRecordStrides[i];
	}

	MemoryRequirements memoryRequirements = {};
	memoryRequirements.ShaderTableSizeInBytes = Math::AlignUp<UINT64>(shaderTableSizeInBytes, D3D12_RAYTRACING_SHADER_TABLE_BYTE_ALIGNMENT);

	return memoryRequirements;
}

void ShaderTable::Upload(RaytracingPipelineState* pRaytracingPipelineState, Buffer* pShaderTableBuffer)
{
	BYTE* pData = pShaderTableBuffer->Map();
	UINT64 offset = 0;

	for (size_t i = 0; i < NumRecordTypes; ++i)
	{
		offset = UploadShaderData(pRaytracingPipelineState, pData, m_ShaderRecords[i], m_ShaderRecordStrides[i]);
		pData += offset;
	}
	pShaderTableBuffer->Unmap();
}

UINT64 ShaderTable::UploadShaderData(RaytracingPipelineState* pRaytracingPipelineState, BYTE* pData, const std::vector<ShaderTable::ShaderRecord>& ShaderRecords, UINT64 ShaderRecordStride)
{
	for (const auto& shaderRecord : ShaderRecords)
	{
		ShaderIdentifier shaderIdentifier = pRaytracingPipelineState->GetShaderIdentifier(shaderRecord.ExportName);
		// Copy the shader identifier
		memcpy(pData, shaderIdentifier.Data, sizeof(ShaderIdentifier));
		// Copy all its resources pointers or values in bulk
		memcpy(pData + sizeof(ShaderIdentifier), shaderRecord.RootArguments.data(), shaderRecord.RootArguments.size() * 8);

		pData += ShaderRecordStride;
	}
	// Return the number of bytes actually written to the output buffer
	return static_cast<UINT64>(ShaderRecords.size()) * ShaderRecordStride;
}