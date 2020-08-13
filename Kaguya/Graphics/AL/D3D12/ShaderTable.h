#pragma once
#include "Shader.h"

class Buffer;

// ShaderRecord = {{Shader Identifier}, {RootArguments}}
struct ShaderRecord
{
	ShaderIdentifier ShaderIdentifier;
	std::vector<void*> RootArguments;
};

class ShaderTable
{
public:
	ShaderTable();

	inline auto GetShaderRecordStride() const { return m_ShaderRecordStride; }

	void AddShaderRecord(ShaderIdentifier ShaderIdentifier, std::vector<void*> RootArguments);
	void Reset();

	void ComputeMemoryRequirements(UINT64* pShaderTableSizeInBytes) const;
	void Generate(Buffer* pShaderTableBuffer);
private:
	std::vector<ShaderRecord> m_ShaderRecords;
	// The stride of a shader record type, this is the maximum record size for each type
	UINT64 m_ShaderRecordStride;
};