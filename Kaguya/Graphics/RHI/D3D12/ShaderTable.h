#pragma once
#include <vector>
#include <Core/Math.h>
#include "Shader.h"

/*
*	========== Miss shader table indexing ==========
	Simple pointer arithmetic
	MissShaderRecordAddress = D3D12_DISPATCH_RAYS_DESC.MissShaderTable.StartAddress + D3D12_DISPATCH_RAYS_DESC.MissShaderTable.StrideInBytes * MissShaderIndex
*/

/*
*	========== Hit group table indexing ==========
	HitGroupRecordAddress = D3D12_DISPATCH_RAYS_DESC.HitGroupTable.StartAddress + D3D12_DISPATCH_RAYS_DESC.HitGroupTable.StrideInBytes * HitGroupEntryIndex

	where HitGroupEntryIndex = 
	(RayContributionToHitGroupIndex + (MultiplierForGeometryContributionToHitGroupIndex * GeometryContributionToHitGroupIndex) + D3D12_RAYTRACING_INSTANCE_DESC.InstanceContributionToHitGroupIndex)

	GeometryContributionToHitGroupIndex is a system generated index of geometry in bottom-level acceleration structure (0,1,2,3..)
*/

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
	{
		constexpr UINT64 RecordSizeInBytes = sizeof(ShaderIdentifier) + sizeof(TRootArguments);

		m_ShaderRecordStride = Math::AlignUp<UINT64>(RecordSizeInBytes, D3D12_RAYTRACING_SHADER_RECORD_BYTE_ALIGNMENT);
	}

	void Clear()
	{
		m_ShaderRecords.clear();
	}

	void Reserve(size_t NumShaderRecords)
	{
		m_ShaderRecords.reserve(NumShaderRecords);
	}

	void Resize(size_t NumShaderRecords)
	{
		m_ShaderRecords.resize(NumShaderRecords);
	}

	ShaderRecord<TRootArguments>& operator[](size_t Index)
	{
		return m_ShaderRecords[Index];
	}

	inline auto GetShaderRecordStride() const { return m_ShaderRecordStride; }

	void AddShaderRecord(ShaderIdentifier ShaderIdentifier, TRootArguments RootArguments)
	{
		ShaderRecord<TRootArguments> ShaderRecord = {};
		ShaderRecord.ShaderIdentifier = ShaderIdentifier;
		ShaderRecord.RootArguments = RootArguments;

		m_ShaderRecords.push_back(ShaderRecord);
	}

	void Reset()
	{
		m_ShaderRecords.clear();
	}

	void ComputeMemoryRequirements(UINT64* pShaderTableSizeInBytes) const
	{
		if (pShaderTableSizeInBytes)
		{
			UINT64 ShaderTableSizeInBytes = static_cast<UINT64>(m_ShaderRecords.size()) * m_ShaderRecordStride;
			ShaderTableSizeInBytes = Math::AlignUp<UINT64>(ShaderTableSizeInBytes, D3D12_RAYTRACING_SHADER_TABLE_BYTE_ALIGNMENT);

			*pShaderTableSizeInBytes = ShaderTableSizeInBytes;
		}
	}

	void Generate(BYTE* pHostMemory)
	{
		if (pHostMemory)
		{
			for (const auto& ShaderRecord : m_ShaderRecords)
			{
				// Copy the shader identifier
				memcpy(pHostMemory, ShaderRecord.ShaderIdentifier.Data, sizeof(ShaderIdentifier));
				// Copy all its resources pointers or values in bulk
				memcpy(pHostMemory + sizeof(ShaderIdentifier), &ShaderRecord.RootArguments, sizeof(TRootArguments));

				pHostMemory += m_ShaderRecordStride;
			}
		}
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
	{
		constexpr UINT64 RecordSizeInBytes = sizeof(ShaderIdentifier);

		m_ShaderRecordStride = Math::AlignUp<UINT64>(RecordSizeInBytes, D3D12_RAYTRACING_SHADER_RECORD_BYTE_ALIGNMENT);
	}

	void Clear()
	{
		m_ShaderRecords.clear();
	}

	void Reserve(size_t NumShaderRecords)
	{
		m_ShaderRecords.reserve(NumShaderRecords);
	}

	void Resize(size_t NumShaderRecords)
	{
		m_ShaderRecords.resize(NumShaderRecords);
	}

	ShaderRecord<void>& operator[](size_t Index)
	{
		return m_ShaderRecords[Index];
	}

	inline auto GetShaderRecordStride() const { return m_ShaderRecordStride; }

	void AddShaderRecord(ShaderIdentifier ShaderIdentifier)
	{
		ShaderRecord<void> ShaderRecord = {};
		ShaderRecord.ShaderIdentifier = ShaderIdentifier;

		m_ShaderRecords.push_back(ShaderRecord);
	}

	void Reset()
	{
		m_ShaderRecords.clear();
	}

	void ComputeMemoryRequirements(UINT64* pShaderTableSizeInBytes) const
	{
		if (pShaderTableSizeInBytes)
		{
			UINT64 ShaderTableSizeInBytes = static_cast<UINT64>(m_ShaderRecords.size()) * m_ShaderRecordStride;
			ShaderTableSizeInBytes = Math::AlignUp<UINT64>(ShaderTableSizeInBytes, D3D12_RAYTRACING_SHADER_TABLE_BYTE_ALIGNMENT);

			*pShaderTableSizeInBytes = ShaderTableSizeInBytes;
		}
	}

	void Generate(BYTE* pHostMemory)
	{
		if (pHostMemory)
		{
			for (const auto& ShaderRecord : m_ShaderRecords)
			{
				// Copy the shader identifier
				memcpy(pHostMemory, ShaderRecord.ShaderIdentifier.Data, sizeof(ShaderIdentifier));

				pHostMemory += m_ShaderRecordStride;
			}
		}
	}
private:
	std::vector<ShaderRecord<void>> m_ShaderRecords;
	// The stride of a shader record type, this is the maximum record size for each type
	UINT64 m_ShaderRecordStride;
};