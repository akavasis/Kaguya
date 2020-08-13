#include "pch.h"
#include "ShaderTable.h"
#include "Buffer.h"

ShaderTable::ShaderTable()
	: m_ShaderRecordStride(0)
{
}

void ShaderTable::AddShaderRecord(ShaderIdentifier ShaderIdentifier, std::vector<void*> RootArguments)
{
	constexpr UINT64 ShaderIdentiferSizeInBytes = sizeof(ShaderIdentifier);
	UINT64 recordSizeInBytes = ShaderIdentiferSizeInBytes;
	recordSizeInBytes += 8 * static_cast<UINT64>(RootArguments.size());
	recordSizeInBytes = Math::AlignUp<UINT64>(recordSizeInBytes, D3D12_RAYTRACING_SHADER_RECORD_BYTE_ALIGNMENT);

	ShaderRecord shaderRecord = {};
	shaderRecord.ShaderIdentifier = ShaderIdentifier;
	shaderRecord.RootArguments = std::move(RootArguments);

	m_ShaderRecords.push_back(shaderRecord);
	m_ShaderRecordStride = std::max(m_ShaderRecordStride, recordSizeInBytes);
}

void ShaderTable::Reset()
{
	m_ShaderRecords.clear();
	m_ShaderRecordStride = 0;
}

void ShaderTable::ComputeMemoryRequirements(UINT64* pShaderTableSizeInBytes) const
{
	assert(pShaderTableSizeInBytes != nullptr);

	UINT64 shaderTableSizeInBytes = 0;
	shaderTableSizeInBytes += static_cast<UINT64>(m_ShaderRecords.size()) * m_ShaderRecordStride;
	shaderTableSizeInBytes = Math::AlignUp<UINT64>(shaderTableSizeInBytes, D3D12_RAYTRACING_SHADER_TABLE_BYTE_ALIGNMENT);

	*pShaderTableSizeInBytes = shaderTableSizeInBytes;
}

void ShaderTable::Generate(Buffer* pShaderTableBuffer)
{
	BYTE* pData = pShaderTableBuffer->Map();
	
	for (const auto& shaderRecord : m_ShaderRecords)
	{
		// Copy the shader identifier
		memcpy(pData, shaderRecord.ShaderIdentifier.Data, sizeof(ShaderIdentifier));
		// Copy all its resources pointers or values in bulk
		memcpy(pData + sizeof(ShaderIdentifier), shaderRecord.RootArguments.data(), shaderRecord.RootArguments.size() * 8);

		pData += m_ShaderRecordStride;
	}

	pShaderTableBuffer->Unmap();
}