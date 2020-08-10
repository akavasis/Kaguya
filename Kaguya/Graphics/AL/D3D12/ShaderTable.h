#pragma once
#include "Shader.h"

class Buffer;
class RaytracingPipelineState;

class ShaderTable
{
public:
	enum RecordType
	{
		RayGeneration,
		Miss,
		HitGroup,
		NumRecordTypes
	};

	struct ShaderRecord
	{
		std::wstring ExportName;
		std::vector<void*> RootArguments;

		UINT64 SizeInBytes;
	};

	struct MemoryRequirements
	{
		UINT64 ShaderTableSizeInBytes;
	};

	ShaderTable();

	inline auto GetShaderRecordSize(RecordType Type) const { return m_ShaderRecordStrides[Type] * m_ShaderRecords[Type].size(); }
	inline auto GetShaderRecordStride(RecordType Type) const { return m_ShaderRecordStrides[Type]; }

	void AddShaderRecord(RecordType Type, LPCWSTR pExportName, std::vector<void*> RootArguments);
	void Reset();

	MemoryRequirements GetShaderTableMemoryRequirements() const;
	void Upload(RaytracingPipelineState* pRaytracingPipelineState, Buffer* pShaderTableBuffer);
private:
	// For each entry, copy the shader identifier followed by its resource pointers and/or root
	// constants in outputData, with a stride in bytes of entrySize, and returns the size in bytes
	// actually written to pData.
	UINT64 UploadShaderData(RaytracingPipelineState* pRaytracingPipelineState,
		BYTE* pData, const std::vector<ShaderRecord>& ShaderRecords, UINT64 ShaderRecordStride);

	std::vector<ShaderRecord> m_ShaderRecords[NumRecordTypes];
	// The stride of a shader record type, this is the maximum record size for each type
	std::array<UINT64, NumRecordTypes> m_ShaderRecordStrides;
};