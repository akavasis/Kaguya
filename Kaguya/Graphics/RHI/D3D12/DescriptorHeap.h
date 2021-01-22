#pragma once

//----------------------------------------------------------------------------------------------------
struct Descriptor
{
	inline bool IsValid() const { return CpuHandle.ptr != NULL; }
	inline bool IsReferencedByShader() const { return GpuHandle.ptr != NULL; }

	D3D12_CPU_DESCRIPTOR_HANDLE CpuHandle = { NULL };
	D3D12_GPU_DESCRIPTOR_HANDLE GpuHandle = { NULL };
	UINT						Index = 0;
};

//----------------------------------------------------------------------------------------------------
class DescriptorHeap
{
public:
	void Create(ID3D12Device* pDevice, std::vector<UINT> Ranges, bool ShaderVisible, D3D12_DESCRIPTOR_HEAP_TYPE Type);

	operator auto() const { return m_DescriptorHeap.Get(); }

	inline auto GetDescriptorFromStart() const { return m_StartDescriptor; }

	Descriptor GetDescriptorAt(INT PartitionIndex, UINT Index) const;
protected:
	auto& GetDescriptorPartitionAt(INT PartitionIndex) { return m_DescriptorPartitions[PartitionIndex]; }
	auto& GetDescriptorPartitionAt(INT PartitionIndex) const { return m_DescriptorPartitions[PartitionIndex]; }

	// Used for partitioning descriptors
	struct DescriptorPartition
	{
		Descriptor StartDescriptor;
		UINT NumDescriptors;
	};

	ID3D12Device* m_pDevice;

	Microsoft::WRL::ComPtr<ID3D12DescriptorHeap>	m_DescriptorHeap;
	Descriptor										m_StartDescriptor;
	UINT											m_DescriptorIncrementSize;
	bool											m_ShaderVisible;
	std::vector<DescriptorPartition>				m_DescriptorPartitions;
};