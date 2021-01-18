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
	void Create(ID3D12Device* pDevice, UINT NumDescriptors, bool ShaderVisible, D3D12_DESCRIPTOR_HEAP_TYPE Type);

	operator auto() const { return m_DescriptorHeap.Get(); }

	inline auto GetApiHandle() const { return m_DescriptorHeap.Get(); }
	inline auto GetDescriptorFromStart() const { return m_StartDescriptor; }

	Descriptor operator[](UINT Index) const;
protected:
	Microsoft::WRL::ComPtr<ID3D12DescriptorHeap>	m_DescriptorHeap;
	Descriptor										m_StartDescriptor;
	UINT											m_DescriptorIncrementSize;
	bool											m_ShaderVisible;
};