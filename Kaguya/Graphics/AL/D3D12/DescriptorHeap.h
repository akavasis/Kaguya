#pragma once
#include <d3d12.h>
#include <wrl/client.h>

#include "../../../Core/Allocator/VariableSizedAllocator.h"

class Device;

struct Descriptor
{
	D3D12_CPU_DESCRIPTOR_HANDLE CPUHandle = { NULL };
	D3D12_GPU_DESCRIPTOR_HANDLE GPUHandle = { NULL };
	UINT HeapIndex = 0;
	UINT IncrementSize = 0;

	inline bool IsValid() const { return CPUHandle.ptr != NULL; }
	inline bool IsReferencedByShader() const { return GPUHandle.ptr != NULL; }
};

struct DescriptorAllocation
{
	Descriptor StartDescriptor = {};
	UINT NumDescriptors = 0;

	Descriptor operator[](INT Index) const;
	inline operator bool() const
	{
		return StartDescriptor.IsValid();
	}
private:
	friend class DescriptorHeap;
	friend class DescriptorAllocator;
	DescriptorHeap* pOwningHeap = nullptr;
};

class DescriptorHeap
{
public:
	DescriptorHeap() = default;
	DescriptorHeap(Device* pDevice, std::vector<UINT> Ranges, bool ShaderVisible, D3D12_DESCRIPTOR_HEAP_TYPE Type);
	virtual ~DescriptorHeap() = default;

	DescriptorHeap(DescriptorHeap&&) noexcept = default;
	DescriptorHeap& operator=(DescriptorHeap&&) noexcept = default;

	DescriptorHeap(const DescriptorHeap&) = delete;
	DescriptorHeap& operator=(const DescriptorHeap&) = delete;

	inline auto GetD3DDescriptorHeap() const { return m_pDescriptorHeap.Get(); }

	std::optional<DescriptorAllocation> Allocate(INT PartitionIndex, UINT NumDescriptors);
	void Free(INT PartitionIndex, DescriptorAllocation& DescriptorAllocation);
protected:
	auto& GetDescriptorPartitionAt(INT Index) { return m_DescriptorPartitions[Index]; }
	auto& GetDescriptorPartitionAt(INT Index) const { return m_DescriptorPartitions[Index]; }
	D3D12_CPU_DESCRIPTOR_HANDLE GetCPUHandleAt(INT PartitionIndex, INT Index) const;
	D3D12_GPU_DESCRIPTOR_HANDLE GetGPUHandleAt(INT PartitionIndex, INT Index) const;

	// Used for partitioning descriptors
	struct DescriptorPartition
	{
		DescriptorAllocation Allocation;
		VariableSizedAllocator Allocator;
	};

	Microsoft::WRL::ComPtr<ID3D12DescriptorHeap> m_pDescriptorHeap;
	UINT m_DescriptorIncrementSize;
	bool m_ShaderVisible;
	std::vector<DescriptorPartition> m_DescriptorPartitions;
};

class CBSRUADescriptorHeap : public DescriptorHeap
{
public:
	enum RangeType
	{
		ConstantBuffer,
		ShaderResource,
		UnorderedAccess,
		NumRangeTypes
	};

	CBSRUADescriptorHeap(Device* pDevice, UINT NumCBDescriptors, UINT NumSRDescriptors, UINT NumUADescriptors, bool ShaderVisible);

	std::optional<DescriptorAllocation> AllocateCBDescriptors(UINT NumDescriptors);
	std::optional<DescriptorAllocation> AllocateSRDescriptors(UINT NumDescriptors);
	std::optional<DescriptorAllocation> AllocateUADescriptors(UINT NumDescriptors);

	D3D12_GPU_DESCRIPTOR_HANDLE GetRangeHandleFromStart(RangeType Type) const
	{
		auto& descriptorPartition = GetDescriptorPartitionAt(INT(Type));
		return descriptorPartition.Allocation.StartDescriptor.GPUHandle;
	}
};

class SamplerDescriptorHeap : public DescriptorHeap
{
public:
	SamplerDescriptorHeap(Device* pDevice, std::vector<UINT> Ranges, bool ShaderVisible);
};

class RenderTargetDescriptorHeap : public DescriptorHeap
{
public:
	RenderTargetDescriptorHeap(Device* pDevice, std::vector<UINT> Ranges);
};

class DepthStencilDescriptorHeap : public DescriptorHeap
{
public:
	DepthStencilDescriptorHeap(Device* pDevice, std::vector<UINT> Ranges);
};