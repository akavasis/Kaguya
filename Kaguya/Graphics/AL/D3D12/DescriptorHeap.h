#pragma once
#include <d3d12.h>
#include <wrl/client.h>
#include "Core/Allocator/VariableSizedAllocator.h"

class Device;
class DescriptorHeap;

struct Descriptor
{
	inline bool IsValid() const { return CPUHandle.ptr != NULL; }
	inline bool IsReferencedByShader() const { return GPUHandle.ptr != NULL; }

	D3D12_CPU_DESCRIPTOR_HANDLE CPUHandle = { NULL };
	D3D12_GPU_DESCRIPTOR_HANDLE GPUHandle = { NULL };
	UINT HeapIndex = 0;
	UINT PartitionHeapIndex = 0;
};

class DescriptorAllocation
{
public:
	// Constructs NULL Descriptor Allocation
	DescriptorAllocation();
	DescriptorAllocation(Descriptor StartDescriptor, UINT NumDescriptors, UINT IncrementSize, INT PartitonIndex, DescriptorHeap* pOwningHeap);

	// Frees this DescriptorAllocation
	~DescriptorAllocation();

	// Only allow move semantics
	DescriptorAllocation(DescriptorAllocation&& rvalue) noexcept;
	DescriptorAllocation& operator=(DescriptorAllocation&& rvalue) noexcept;

	// No copy
	DescriptorAllocation(const DescriptorAllocation&) = delete;
	DescriptorAllocation& operator=(const DescriptorAllocation&) = delete;

	Descriptor operator[](INT Index) const;
	inline operator bool() const
	{
		return StartDescriptor.IsValid();
	}

	inline auto GetStartDescriptor() const { return StartDescriptor; }
	inline auto GetNumDescriptors() const { return NumDescriptors; }
	inline auto GetIncrementSize() const { return IncrementSize; }
private:
	friend class DescriptorHeap;
	friend class DescriptorAllocator;

	Descriptor StartDescriptor;
	UINT NumDescriptors;
	UINT IncrementSize;
	INT PartitionIndex;
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

	DescriptorAllocation Allocate(INT PartitionIndex, UINT NumDescriptors);
	void Free(INT PartitionIndex, DescriptorAllocation&& DescriptorAllocation);
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

	DescriptorAllocation AllocateCBDescriptors(UINT NumDescriptors);
	DescriptorAllocation AllocateSRDescriptors(UINT NumDescriptors);
	DescriptorAllocation AllocateUADescriptors(UINT NumDescriptors);

	Descriptor GetRangeHandleFromStart(RangeType Type) const
	{
		auto& descriptorPartition = GetDescriptorPartitionAt(INT(Type));
		return descriptorPartition.Allocation[0];
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