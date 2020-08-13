#pragma once
#include <vector>
#include "Shader.h"
#include "Buffer.h"

// ShaderRecord = {{Shader Identifier}, {RootArguments}}
template<typename TRootArguments>
struct ShaderRecord
{
	ShaderIdentifier ShaderIdentifier;
	TRootArguments RootArguments;
};

template<>
struct ShaderRecord<void>
{
	ShaderIdentifier ShaderIdentifier;
};

template<typename TRootArguments>
class ShaderTable
{
public:
	ShaderTable()
		: m_ShaderRecordStride(0)
	{
	}

	inline auto GetShaderRecordStride() const { return m_ShaderRecordStride; }

	void AddShaderRecord(ShaderIdentifier ShaderIdentifier, TRootArguments RootArguments)
	{
		constexpr UINT64 ShaderIdentiferSizeInBytes = sizeof(ShaderIdentifier);
		UINT64 recordSizeInBytes = ShaderIdentiferSizeInBytes;
		recordSizeInBytes += sizeof(TRootArguments);
		recordSizeInBytes = Math::AlignUp<UINT64>(recordSizeInBytes, D3D12_RAYTRACING_SHADER_RECORD_BYTE_ALIGNMENT);

		ShaderRecord<TRootArguments> shaderRecord = {};
		shaderRecord.ShaderIdentifier = ShaderIdentifier;
		shaderRecord.RootArguments = RootArguments;

		m_ShaderRecords.push_back(shaderRecord);
		m_ShaderRecordStride = std::max<UINT64>(m_ShaderRecordStride, recordSizeInBytes);
	}

	void Reset()
	{
		m_ShaderRecords.clear();
		m_ShaderRecordStride = 0;
	}

	void ComputeMemoryRequirements(UINT64* pShaderTableSizeInBytes) const
	{
		assert(pShaderTableSizeInBytes != nullptr);

		UINT64 shaderTableSizeInBytes = 0;
		shaderTableSizeInBytes += static_cast<UINT64>(m_ShaderRecords.size()) * m_ShaderRecordStride;
		shaderTableSizeInBytes = Math::AlignUp<UINT64>(shaderTableSizeInBytes, D3D12_RAYTRACING_SHADER_TABLE_BYTE_ALIGNMENT);

		*pShaderTableSizeInBytes = shaderTableSizeInBytes;
	}

	void Generate(Buffer* pShaderTableBuffer)
	{
		BYTE* pData = pShaderTableBuffer->Map();

		for (const auto& shaderRecord : m_ShaderRecords)
		{
			// Copy the shader identifier
			memcpy(pData, shaderRecord.ShaderIdentifier.Data, sizeof(ShaderIdentifier));
			// Copy all its resources pointers or values in bulk
			memcpy(pData + sizeof(ShaderIdentifier), &shaderRecord.RootArguments, sizeof(TRootArguments));

			pData += m_ShaderRecordStride;
		}

		pShaderTableBuffer->Unmap();
	}
private:
	std::vector<ShaderRecord<TRootArguments>> m_ShaderRecords;
	// The stride of a shader record type, this is the maximum record size for each type
	UINT64 m_ShaderRecordStride;
};

template<>
class ShaderTable<void>
{
public:
	ShaderTable()
		: m_ShaderRecordStride(0)
	{
	}

	inline auto GetShaderRecordStride() const { return m_ShaderRecordStride; }

	void AddShaderRecord(ShaderIdentifier ShaderIdentifier)
	{
		constexpr UINT64 ShaderIdentiferSizeInBytes = sizeof(ShaderIdentifier);
		UINT64 recordSizeInBytes = ShaderIdentiferSizeInBytes;
		recordSizeInBytes = Math::AlignUp<UINT64>(recordSizeInBytes, D3D12_RAYTRACING_SHADER_RECORD_BYTE_ALIGNMENT);

		ShaderRecord<void> shaderRecord = {};
		shaderRecord.ShaderIdentifier = ShaderIdentifier;

		m_ShaderRecords.push_back(shaderRecord);
		m_ShaderRecordStride = std::max<UINT64>(m_ShaderRecordStride, recordSizeInBytes);
	}

	void Reset()
	{
		m_ShaderRecords.clear();
		m_ShaderRecordStride = 0;
	}

	void ComputeMemoryRequirements(UINT64* pShaderTableSizeInBytes) const
	{
		assert(pShaderTableSizeInBytes != nullptr);

		UINT64 shaderTableSizeInBytes = 0;
		shaderTableSizeInBytes += static_cast<UINT64>(m_ShaderRecords.size()) * m_ShaderRecordStride;
		shaderTableSizeInBytes = Math::AlignUp<UINT64>(shaderTableSizeInBytes, D3D12_RAYTRACING_SHADER_TABLE_BYTE_ALIGNMENT);

		*pShaderTableSizeInBytes = shaderTableSizeInBytes;
	}

	void Generate(Buffer* pShaderTableBuffer)
	{
		BYTE* pData = pShaderTableBuffer->Map();

		for (const auto& shaderRecord : m_ShaderRecords)
		{
			// Copy the shader identifier
			memcpy(pData, shaderRecord.ShaderIdentifier.Data, sizeof(ShaderIdentifier));

			pData += m_ShaderRecordStride;
		}

		pShaderTableBuffer->Unmap();
	}
private:
	std::vector<ShaderRecord<void>> m_ShaderRecords;
	// The stride of a shader record type, this is the maximum record size for each type
	UINT64 m_ShaderRecordStride;
};