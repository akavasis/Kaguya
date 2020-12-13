#pragma once
#include <d3d12.h>
#include "d3dx12.h"
#include <wrl/client.h>
#include <optional>
#include <vector>

#include "Texture.h"
#include "Buffer.h"

//----------------------------------------------------------------------------------------------------
class Device;
class DescriptorHeap;

//----------------------------------------------------------------------------------------------------
struct Descriptor
{
	D3D12_CPU_DESCRIPTOR_HANDLE CpuHandle = { NULL };
	D3D12_GPU_DESCRIPTOR_HANDLE GpuHandle = { NULL };
	UINT						HeapIndex = 0;

	inline bool IsValid() const { return CpuHandle.ptr != NULL; }
	inline bool IsReferencedByShader() const { return GpuHandle.ptr != NULL; }
};

//----------------------------------------------------------------------------------------------------
class DescriptorHeap
{
public:
	DescriptorHeap() = default;
	DescriptorHeap(ID3D12Device* pDevice, std::vector<UINT> Ranges, bool ShaderVisible, D3D12_DESCRIPTOR_HEAP_TYPE Type);

	inline auto GetApiHandle() const { return m_pDescriptorHeap.Get(); }
	inline auto GetDescriptorFromStart() const { return m_StartDescriptor; }
protected:
	auto& GetDescriptorPartitionAt(INT PartitionIndex) { return m_DescriptorPartitions[PartitionIndex]; }
	auto& GetDescriptorPartitionAt(INT PartitionIndex) const { return m_DescriptorPartitions[PartitionIndex]; }
	Descriptor GetDescriptorAt(INT PartitionIndex, UINT HeapIndex) const;

	// Used for partitioning descriptors
	struct DescriptorPartition
	{
		Descriptor StartDescriptor;
		UINT NumDescriptors;
	};

	ID3D12Device* m_pDevice;

	Microsoft::WRL::ComPtr<ID3D12DescriptorHeap> m_pDescriptorHeap;
	Descriptor m_StartDescriptor;
	UINT m_DescriptorIncrementSize;
	bool m_ShaderVisible;
	std::vector<DescriptorPartition> m_DescriptorPartitions;
};

//----------------------------------------------------------------------------------------------------
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

	CBSRUADescriptorHeap() = default;
	CBSRUADescriptorHeap(ID3D12Device* pDevice, UINT NumCBDescriptors, UINT NumSRDescriptors, UINT NumUADescriptors, bool ShaderVisible);

	// Returns a hash value that can be used for indexing Descriptor into hash tables
	UINT64 AssignCBDescriptor(UINT HeapIndex, Buffer* pBuffer) { __debugbreak(); return 0; } // TODO: Implement 
	UINT64 AssignSRDescriptor(UINT HeapIndex, Buffer* pBuffer);
	UINT64 AssignUADescriptor(UINT HeapIndex, Buffer* pBuffer) { __debugbreak(); return 0; } // TODO: Implement 

	UINT64 AssignSRDescriptor(UINT HeapIndex, Texture* pTexture, std::optional<UINT> MostDetailedMip = {}, std::optional<UINT> MipLevels = {});
	UINT64 AssignUADescriptor(UINT HeapIndex, Texture* pTexture, std::optional<UINT> ArraySlice = {}, std::optional<UINT> MipSlice = {});

	Descriptor GetCBDescriptorAt(UINT HeapIndex) const { return DescriptorHeap::GetDescriptorAt(ConstantBuffer, HeapIndex); }
	Descriptor GetSRDescriptorAt(UINT HeapIndex) const { return DescriptorHeap::GetDescriptorAt(ShaderResource, HeapIndex); }
	Descriptor GetUADescriptorAt(UINT HeapIndex) const { return DescriptorHeap::GetDescriptorAt(UnorderedAccess, HeapIndex); }

	static D3D12_SHADER_RESOURCE_VIEW_DESC GetShaderResourceViewDesc(Buffer* pBuffer);
	static D3D12_UNORDERED_ACCESS_VIEW_DESC GetUnorderedAccessViewDesc(Buffer* pBuffer) { __debugbreak(); } // TODO: Implement 

	static D3D12_SHADER_RESOURCE_VIEW_DESC GetShaderResourceViewDesc(Texture* pTexture, std::optional<UINT> MostDetailedMip = {}, std::optional<UINT> MipLevels = {});
	static D3D12_UNORDERED_ACCESS_VIEW_DESC GetUnorderedAccessViewDesc(Texture* pTexture, std::optional<UINT> ArraySlice = {}, std::optional<UINT> MipSlice = {});

	static UINT64 GetHashValue(const D3D12_SHADER_RESOURCE_VIEW_DESC& Desc);
	static UINT64 GetHashValue(const D3D12_UNORDERED_ACCESS_VIEW_DESC& Desc);
};

//----------------------------------------------------------------------------------------------------
class SamplerDescriptorHeap : public DescriptorHeap
{
public:
	class Entry
	{
	public:
		Entry(ID3D12Device* pDevice, Descriptor DestDescriptor)
			: pDevice(pDevice), DestDescriptor(DestDescriptor)
		{
		}

		// void operator=(Sampler* pSampler);
	private:
		ID3D12Device* pDevice;
		Descriptor DestDescriptor;
	};

	SamplerDescriptorHeap() = default;
	SamplerDescriptorHeap(ID3D12Device* pDevice, UINT NumDescriptors, bool ShaderVisible);
};

//----------------------------------------------------------------------------------------------------
class RenderTargetDescriptorHeap : public DescriptorHeap
{
public:
	class Entry
	{
	public:
		Entry(ID3D12Device* pDevice, Descriptor DestDescriptor)
			: pDevice(pDevice), DestDescriptor(DestDescriptor), HashValue(0)
		{
		}

		void operator=(Texture* pTexture);
		inline auto GetHashValue() { return HashValue; }

		std::optional<UINT> ArraySlice;
		std::optional<UINT> MipSlice;
		std::optional<UINT> ArraySize;
	private:
		ID3D12Device* pDevice;
		Descriptor DestDescriptor;
		UINT64 HashValue;
	};

	RenderTargetDescriptorHeap() = default;
	RenderTargetDescriptorHeap(ID3D12Device* pDevice, UINT NumDescriptors);

	Entry operator[](UINT Index)
	{
		return Entry(m_pDevice, GetDescriptorAt(Index));
	}

	Descriptor GetDescriptorAt(UINT HeapIndex) const
	{
		return DescriptorHeap::GetDescriptorAt(0, HeapIndex);
	}

	static D3D12_RENDER_TARGET_VIEW_DESC GetRenderTargetViewDesc(Texture* pTexture, std::optional<UINT> ArraySlice = {}, std::optional<UINT> MipSlice = {}, std::optional<UINT> ArraySize = {});
	static UINT64 GetHashValue(const D3D12_RENDER_TARGET_VIEW_DESC& Desc);
};

//----------------------------------------------------------------------------------------------------
class DepthStencilDescriptorHeap : public DescriptorHeap
{
public:
	class Entry
	{
	public:
		Entry(ID3D12Device* pDevice, Descriptor DestDescriptor)
			: pDevice(pDevice), DestDescriptor(DestDescriptor), HashValue(0)
		{
		}

		void operator=(Texture* pTexture);
		inline auto GetHashValue() { return HashValue; }

		std::optional<UINT> ArraySlice;
		std::optional<UINT> MipSlice;
		std::optional<UINT> ArraySize;
	private:
		ID3D12Device* pDevice;
		Descriptor DestDescriptor;
		UINT64 HashValue;
	};

	DepthStencilDescriptorHeap() = default;
	DepthStencilDescriptorHeap(ID3D12Device* pDevice, UINT NumDescriptors);

	Entry operator[](UINT Index)
	{
		return Entry(m_pDevice, GetDescriptorAt(Index));
	}

	Descriptor GetDescriptorAt(UINT HeapIndex) const
	{
		return DescriptorHeap::GetDescriptorAt(0, HeapIndex);
	}

	static D3D12_DEPTH_STENCIL_VIEW_DESC GetDepthStencilViewDesc(Texture* pTexture, std::optional<UINT> ArraySlice = {}, std::optional<UINT> MipSlice = {}, std::optional<UINT> ArraySize = {});
	static UINT64 GetHashValue(const D3D12_DEPTH_STENCIL_VIEW_DESC& Desc);
};