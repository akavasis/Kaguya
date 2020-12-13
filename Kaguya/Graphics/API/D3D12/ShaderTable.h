#pragma once
#include <vector>
#include <Math/MathLibrary.h>
#include "Shader.h"
#include "Buffer.h"

// ShaderRecord = {{Shader Identifier}, {RootArguments}}
template<typename TRootArguments>
struct ShaderRecord
{
	ShaderIdentifier	ShaderIdentifier;
	TRootArguments		RootArguments;
};

template<>
struct ShaderRecord<void>
{
	ShaderIdentifier	ShaderIdentifier;
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
		UINT64 RecordSizeInBytes = ShaderIdentiferSizeInBytes;
		RecordSizeInBytes += sizeof(TRootArguments);
		RecordSizeInBytes = Math::AlignUp<UINT64>(RecordSizeInBytes, D3D12_RAYTRACING_SHADER_RECORD_BYTE_ALIGNMENT);

		ShaderRecord<TRootArguments> ShaderRecord = {};
		ShaderRecord.ShaderIdentifier = ShaderIdentifier;
		ShaderRecord.RootArguments = RootArguments;

		m_ShaderRecords.push_back(ShaderRecord);
		m_ShaderRecordStride = std::max<UINT64>(m_ShaderRecordStride, RecordSizeInBytes);
	}

	void Reset()
	{
		m_ShaderRecords.clear();
		m_ShaderRecordStride = 0;
	}

	void ComputeMemoryRequirements(UINT64* pShaderTableSizeInBytes) const
	{
		if (pShaderTableSizeInBytes)
		{
			UINT64 ShaderTableSizeInBytes = 0;
			ShaderTableSizeInBytes += static_cast<UINT64>(m_ShaderRecords.size()) * m_ShaderRecordStride;
			ShaderTableSizeInBytes = Math::AlignUp<UINT64>(ShaderTableSizeInBytes, D3D12_RAYTRACING_SHADER_TABLE_BYTE_ALIGNMENT);

			*pShaderTableSizeInBytes = ShaderTableSizeInBytes;
		}
	}

	void Generate(Buffer* pShaderTableBuffer)
	{
		BYTE* pData = pShaderTableBuffer->Map();

		for (const auto& ShaderRecord : m_ShaderRecords)
		{
			// Copy the shader identifier
			memcpy(pData, ShaderRecord.ShaderIdentifier.Data, sizeof(ShaderIdentifier));
			// Copy all its resources pointers or values in bulk
			memcpy(pData + sizeof(ShaderIdentifier), &ShaderRecord.RootArguments, sizeof(TRootArguments));

			pData += m_ShaderRecordStride;
		}

		pShaderTableBuffer->Unmap();
	}

private:
	std::vector<ShaderRecord<TRootArguments>>	m_ShaderRecords;
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
		constexpr auto ShaderIdentiferSizeInBytes = sizeof(ShaderIdentifier);
		UINT64 RecordSizeInBytes = ShaderIdentiferSizeInBytes;
		RecordSizeInBytes = Math::AlignUp<UINT64>(RecordSizeInBytes, D3D12_RAYTRACING_SHADER_RECORD_BYTE_ALIGNMENT);

		ShaderRecord<void> ShaderRecord = {};
		ShaderRecord.ShaderIdentifier = ShaderIdentifier;

		m_ShaderRecords.push_back(ShaderRecord);
		m_ShaderRecordStride = std::max<UINT64>(m_ShaderRecordStride, RecordSizeInBytes);
	}

	void Reset()
	{
		m_ShaderRecords.clear();
		m_ShaderRecordStride = 0;
	}

	void ComputeMemoryRequirements(UINT64* pShaderTableSizeInBytes) const
	{
		if (pShaderTableSizeInBytes)
		{
			UINT64 ShaderTableSizeInBytes = 0;
			ShaderTableSizeInBytes += static_cast<UINT64>(m_ShaderRecords.size()) * m_ShaderRecordStride;
			ShaderTableSizeInBytes = Math::AlignUp<UINT64>(ShaderTableSizeInBytes, D3D12_RAYTRACING_SHADER_TABLE_BYTE_ALIGNMENT);

			*pShaderTableSizeInBytes = ShaderTableSizeInBytes;
		}
	}

	void Generate(Buffer* pShaderTableBuffer)
	{
		BYTE* pData = pShaderTableBuffer->Map();

		for (const auto& ShaderRecord : m_ShaderRecords)
		{
			// Copy the shader identifier
			memcpy(pData, ShaderRecord.ShaderIdentifier.Data, sizeof(ShaderIdentifier));

			pData += m_ShaderRecordStride;
		}

		pShaderTableBuffer->Unmap();
	}

private:
	std::vector<ShaderRecord<void>> m_ShaderRecords;
	// The stride of a shader record type, this is the maximum record size for each type
	UINT64 m_ShaderRecordStride;
};