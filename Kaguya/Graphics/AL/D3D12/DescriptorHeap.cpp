#include "pch.h"
#include "DescriptorHeap.h"
#include "Device.h"

namespace std
{
	template<>
	struct hash<D3D12_SHADER_RESOURCE_VIEW_DESC>
	{
		inline std::size_t operator()(const D3D12_SHADER_RESOURCE_VIEW_DESC& Desc) const noexcept
		{
			std::size_t seed = 0;

			hash_combine(seed, Desc.Format);
			hash_combine(seed, Desc.ViewDimension);
			hash_combine(seed, Desc.Shader4ComponentMapping);

			switch (Desc.ViewDimension)
			{
			case D3D12_SRV_DIMENSION_BUFFER:
				hash_combine(seed, Desc.Buffer.FirstElement);
				hash_combine(seed, Desc.Buffer.NumElements);
				hash_combine(seed, Desc.Buffer.StructureByteStride);
				hash_combine(seed, Desc.Buffer.Flags);
				break;
			case D3D12_SRV_DIMENSION_TEXTURE1D:
				hash_combine(seed, Desc.Texture1D.MostDetailedMip);
				hash_combine(seed, Desc.Texture1D.MipLevels);
				hash_combine(seed, Desc.Texture1D.ResourceMinLODClamp);
				break;
			case D3D12_SRV_DIMENSION_TEXTURE1DARRAY:
				hash_combine(seed, Desc.Texture1DArray.MostDetailedMip);
				hash_combine(seed, Desc.Texture1DArray.MipLevels);
				hash_combine(seed, Desc.Texture1DArray.FirstArraySlice);
				hash_combine(seed, Desc.Texture1DArray.ArraySize);
				hash_combine(seed, Desc.Texture1DArray.ResourceMinLODClamp);
				break;
			case D3D12_SRV_DIMENSION_TEXTURE2D:
				hash_combine(seed, Desc.Texture2D.MostDetailedMip);
				hash_combine(seed, Desc.Texture2D.MipLevels);
				hash_combine(seed, Desc.Texture2D.PlaneSlice);
				hash_combine(seed, Desc.Texture2D.ResourceMinLODClamp);
				break;
			case D3D12_SRV_DIMENSION_TEXTURE2DARRAY:
				hash_combine(seed, Desc.Texture2DArray.MostDetailedMip);
				hash_combine(seed, Desc.Texture2DArray.MipLevels);
				hash_combine(seed, Desc.Texture2DArray.FirstArraySlice);
				hash_combine(seed, Desc.Texture2DArray.ArraySize);
				hash_combine(seed, Desc.Texture2DArray.PlaneSlice);
				hash_combine(seed, Desc.Texture2DArray.ResourceMinLODClamp);
				break;
			case D3D12_SRV_DIMENSION_TEXTURE2DMS:
				//                hash_combine(seed, Desc.Texture2DMS.UnusedField_NothingToDefine);
				break;
			case D3D12_SRV_DIMENSION_TEXTURE2DMSARRAY:
				hash_combine(seed, Desc.Texture2DMSArray.FirstArraySlice);
				hash_combine(seed, Desc.Texture2DMSArray.ArraySize);
				break;
			case D3D12_SRV_DIMENSION_TEXTURE3D:
				hash_combine(seed, Desc.Texture3D.MostDetailedMip);
				hash_combine(seed, Desc.Texture3D.MipLevels);
				hash_combine(seed, Desc.Texture3D.ResourceMinLODClamp);
				break;
			case D3D12_SRV_DIMENSION_TEXTURECUBE:
				hash_combine(seed, Desc.TextureCube.MostDetailedMip);
				hash_combine(seed, Desc.TextureCube.MipLevels);
				hash_combine(seed, Desc.TextureCube.ResourceMinLODClamp);
				break;
			case D3D12_SRV_DIMENSION_TEXTURECUBEARRAY:
				hash_combine(seed, Desc.TextureCubeArray.MostDetailedMip);
				hash_combine(seed, Desc.TextureCubeArray.MipLevels);
				hash_combine(seed, Desc.TextureCubeArray.First2DArrayFace);
				hash_combine(seed, Desc.TextureCubeArray.NumCubes);
				hash_combine(seed, Desc.TextureCubeArray.ResourceMinLODClamp);
				break;
			case D3D12_SRV_DIMENSION_RAYTRACING_ACCELERATION_STRUCTURE:
				hash_combine(seed, Desc.RaytracingAccelerationStructure.Location);
				break;
			}

			return seed;
		}
	};

	template<>
	struct hash<D3D12_UNORDERED_ACCESS_VIEW_DESC>
	{
		inline std::size_t operator()(const D3D12_UNORDERED_ACCESS_VIEW_DESC& uavDesc) const noexcept
		{
			std::size_t seed = 0;

			hash_combine(seed, uavDesc.Format);
			hash_combine(seed, uavDesc.ViewDimension);

			switch (uavDesc.ViewDimension)
			{
			case D3D12_UAV_DIMENSION_BUFFER:
				hash_combine(seed, uavDesc.Buffer.FirstElement);
				hash_combine(seed, uavDesc.Buffer.NumElements);
				hash_combine(seed, uavDesc.Buffer.StructureByteStride);
				hash_combine(seed, uavDesc.Buffer.CounterOffsetInBytes);
				hash_combine(seed, uavDesc.Buffer.Flags);
				break;
			case D3D12_UAV_DIMENSION_TEXTURE1D:
				hash_combine(seed, uavDesc.Texture1D.MipSlice);
				break;
			case D3D12_UAV_DIMENSION_TEXTURE1DARRAY:
				hash_combine(seed, uavDesc.Texture1DArray.MipSlice);
				hash_combine(seed, uavDesc.Texture1DArray.FirstArraySlice);
				hash_combine(seed, uavDesc.Texture1DArray.ArraySize);
				break;
			case D3D12_UAV_DIMENSION_TEXTURE2D:
				hash_combine(seed, uavDesc.Texture2D.MipSlice);
				hash_combine(seed, uavDesc.Texture2D.PlaneSlice);
				break;
			case D3D12_UAV_DIMENSION_TEXTURE2DARRAY:
				hash_combine(seed, uavDesc.Texture2DArray.MipSlice);
				hash_combine(seed, uavDesc.Texture2DArray.FirstArraySlice);
				hash_combine(seed, uavDesc.Texture2DArray.ArraySize);
				hash_combine(seed, uavDesc.Texture2DArray.PlaneSlice);
				break;
			case D3D12_UAV_DIMENSION_TEXTURE3D:
				hash_combine(seed, uavDesc.Texture3D.MipSlice);
				hash_combine(seed, uavDesc.Texture3D.FirstWSlice);
				hash_combine(seed, uavDesc.Texture3D.WSize);
				break;
			}

			return seed;
		}
	};

	template<>
	struct hash<D3D12_RENDER_TARGET_VIEW_DESC>
	{
		inline std::size_t operator()(const D3D12_RENDER_TARGET_VIEW_DESC& Desc) const noexcept
		{
			std::size_t seed = 0;

			hash_combine(seed, Desc.Format);
			hash_combine(seed, Desc.ViewDimension);

			switch (Desc.ViewDimension)
			{
			case D3D12_RTV_DIMENSION_BUFFER:
				hash_combine(seed, Desc.Buffer.FirstElement);
				hash_combine(seed, Desc.Buffer.NumElements);
				break;
			case D3D12_RTV_DIMENSION_TEXTURE1D:
				hash_combine(seed, Desc.Texture1D.MipSlice);
				break;
			case D3D12_RTV_DIMENSION_TEXTURE1DARRAY:
				hash_combine(seed, Desc.Texture1DArray.MipSlice);
				hash_combine(seed, Desc.Texture1DArray.FirstArraySlice);
				hash_combine(seed, Desc.Texture1DArray.ArraySize);
				break;
			case D3D12_RTV_DIMENSION_TEXTURE2D:
				hash_combine(seed, Desc.Texture2D.MipSlice);
				hash_combine(seed, Desc.Texture2D.PlaneSlice);
				break;
			case D3D12_RTV_DIMENSION_TEXTURE2DARRAY:
				hash_combine(seed, Desc.Texture2DArray.MipSlice);
				hash_combine(seed, Desc.Texture2DArray.FirstArraySlice);
				hash_combine(seed, Desc.Texture2DArray.ArraySize);
				hash_combine(seed, Desc.Texture2DArray.PlaneSlice);
				break;
			case D3D12_RTV_DIMENSION_TEXTURE2DMS:
				hash_combine(seed, Desc.Texture2DMS.UnusedField_NothingToDefine);
				break;
			case D3D12_RTV_DIMENSION_TEXTURE2DMSARRAY:
				hash_combine(seed, Desc.Texture2DMSArray.FirstArraySlice);
				hash_combine(seed, Desc.Texture2DMSArray.ArraySize);
				break;
			case D3D12_RTV_DIMENSION_TEXTURE3D:
				hash_combine(seed, Desc.Texture3D.MipSlice);
				hash_combine(seed, Desc.Texture3D.FirstWSlice);
				hash_combine(seed, Desc.Texture3D.WSize);
				break;
			}

			return seed;
		}
	};

	template<>
	struct hash<D3D12_DEPTH_STENCIL_VIEW_DESC>
	{
		inline std::size_t operator()(const D3D12_DEPTH_STENCIL_VIEW_DESC& Desc) const noexcept
		{
			std::size_t seed = 0;

			hash_combine(seed, Desc.Format);
			hash_combine(seed, Desc.ViewDimension);
			hash_combine(seed, Desc.Flags);

			switch (Desc.ViewDimension)
			{
			case D3D12_DSV_DIMENSION_TEXTURE1D:
				hash_combine(seed, Desc.Texture1D.MipSlice);
				break;
			case D3D12_DSV_DIMENSION_TEXTURE1DARRAY:
				hash_combine(seed, Desc.Texture1DArray.MipSlice);
				hash_combine(seed, Desc.Texture1DArray.FirstArraySlice);
				hash_combine(seed, Desc.Texture1DArray.ArraySize);
				break;
			case D3D12_DSV_DIMENSION_TEXTURE2D:
				hash_combine(seed, Desc.Texture2D.MipSlice);
				break;
			case D3D12_DSV_DIMENSION_TEXTURE2DARRAY:
				hash_combine(seed, Desc.Texture2DArray.MipSlice);
				hash_combine(seed, Desc.Texture2DArray.FirstArraySlice);
				hash_combine(seed, Desc.Texture2DArray.ArraySize);
				break;
			case D3D12_DSV_DIMENSION_TEXTURE2DMS:
				hash_combine(seed, Desc.Texture2DMS.UnusedField_NothingToDefine);
				break;
			case D3D12_DSV_DIMENSION_TEXTURE2DMSARRAY:
				hash_combine(seed, Desc.Texture2DMSArray.FirstArraySlice);
				hash_combine(seed, Desc.Texture2DMSArray.ArraySize);
				break;
			}

			return seed;
		}
	};
}

DescriptorHeap::DescriptorHeap(Device* pDevice, std::vector<UINT> Ranges, bool ShaderVisible, D3D12_DESCRIPTOR_HEAP_TYPE Type)
	: m_pDevice(pDevice)
{
	// If you recorded a CPU descriptor handle into the command list (render target or depth stencil) then that descriptor can be reused immediately after the Set call, 
	// if you recorded a GPU descriptor handle into the command list (everything else) then that descriptor cannot be reused until gpu is done referencing them
	m_ShaderVisible = ShaderVisible;

	D3D12_DESCRIPTOR_HEAP_DESC desc = {};
	desc.Type = Type;
	desc.NumDescriptors = std::accumulate(Ranges.begin(), Ranges.end(), 0);
	desc.Flags = m_ShaderVisible ? D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE : D3D12_DESCRIPTOR_HEAP_FLAG_NONE;
	desc.NodeMask = 0;

	ThrowCOMIfFailed(pDevice->GetD3DDevice()->CreateDescriptorHeap(&desc, IID_PPV_ARGS(&m_pDescriptorHeap)));
	m_DescriptorIncrementSize = pDevice->GetDescriptorIncrementSize(Type);

	D3D12_CPU_DESCRIPTOR_HANDLE CpuHandle = m_pDescriptorHeap->GetCPUDescriptorHandleForHeapStart();
	D3D12_GPU_DESCRIPTOR_HANDLE GpuHandle = {};
	if (m_ShaderVisible)
		GpuHandle = m_pDescriptorHeap->GetGPUDescriptorHandleForHeapStart();
	m_StartDescriptor = { CpuHandle, GpuHandle, 0 };

	UINT heapIndex = 0;
	m_DescriptorPartitions.resize(Ranges.size());
	for (std::size_t i = 0; i < Ranges.size(); ++i)
	{
		D3D12_CPU_DESCRIPTOR_HANDLE cpuHandle = {}; CD3DX12_CPU_DESCRIPTOR_HANDLE::InitOffsetted(cpuHandle, CpuHandle, heapIndex, m_DescriptorIncrementSize);
		D3D12_GPU_DESCRIPTOR_HANDLE gpuHandle = {}; CD3DX12_GPU_DESCRIPTOR_HANDLE::InitOffsetted(gpuHandle, GpuHandle, heapIndex, m_DescriptorIncrementSize);

		m_DescriptorPartitions[i].StartDescriptor = { cpuHandle, gpuHandle, heapIndex };
		m_DescriptorPartitions[i].NumDescriptors = Ranges[i];

		heapIndex += Ranges[i];
	}
}

Descriptor DescriptorHeap::GetDescriptorAt(INT PartitionIndex, UINT HeapIndex) const
{
	const auto& partition = GetDescriptorPartitionAt(PartitionIndex);
	D3D12_CPU_DESCRIPTOR_HANDLE cpuHandle = {}; CD3DX12_CPU_DESCRIPTOR_HANDLE::InitOffsetted(cpuHandle, partition.StartDescriptor.CPUHandle, HeapIndex, m_DescriptorIncrementSize);
	D3D12_GPU_DESCRIPTOR_HANDLE gpuHandle = {}; CD3DX12_GPU_DESCRIPTOR_HANDLE::InitOffsetted(gpuHandle, partition.StartDescriptor.GPUHandle, HeapIndex, m_DescriptorIncrementSize);

	return { cpuHandle, gpuHandle, HeapIndex };
}

//----------------------------------------------------------------------------------------------------
CBSRUADescriptorHeap::CBSRUADescriptorHeap(Device* pDevice, UINT NumCBDescriptors, UINT NumSRDescriptors, UINT NumUADescriptors, bool ShaderVisible)
	: DescriptorHeap(pDevice, { NumCBDescriptors, NumSRDescriptors, NumUADescriptors }, ShaderVisible, D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV)
{
}

UINT64 CBSRUADescriptorHeap::AssignSRDescriptor(UINT HeapIndex, DeviceBuffer* pBuffer)
{
	Descriptor DestDescriptor = GetDescriptorAt(ShaderResource, HeapIndex);
	D3D12_SHADER_RESOURCE_VIEW_DESC desc = GetShaderResourceViewDesc(pBuffer);
	if (EnumMaskBitSet(pBuffer->GetBindFlags(), DeviceResource::BindFlags::AccelerationStructure))
	{
		m_pDevice->GetD3DDevice()->CreateShaderResourceView(nullptr, &desc, DestDescriptor.CPUHandle);
	}
	else
	{
		m_pDevice->GetD3DDevice()->CreateShaderResourceView(pBuffer->GetD3DResource(), &desc, DestDescriptor.CPUHandle);
	}
	return GetHashValue(desc);
}

UINT64 CBSRUADescriptorHeap::AssignSRDescriptor(UINT HeapIndex, DeviceTexture* pTexture, std::optional<UINT> MostDetailedMip, std::optional<UINT> MipLevels)
{
	Descriptor DestDescriptor = GetDescriptorAt(ShaderResource, HeapIndex);
	D3D12_SHADER_RESOURCE_VIEW_DESC desc = GetShaderResourceViewDesc(pTexture, MostDetailedMip, MipLevels);
	m_pDevice->GetD3DDevice()->CreateShaderResourceView(pTexture->GetD3DResource(), &desc, DestDescriptor.CPUHandle);
	return GetHashValue(desc);
}

UINT64 CBSRUADescriptorHeap::AssignUADescriptor(UINT HeapIndex, DeviceTexture* pTexture, std::optional<UINT> ArraySlice, std::optional<UINT> MipSlice)
{
	Descriptor DestDescriptor = GetDescriptorAt(UnorderedAccess, HeapIndex);
	D3D12_UNORDERED_ACCESS_VIEW_DESC desc = GetUnorderedAccessViewDesc(pTexture, ArraySlice, MipSlice);
	m_pDevice->GetD3DDevice()->CreateUnorderedAccessView(pTexture->GetD3DResource(), nullptr, &desc, DestDescriptor.CPUHandle);
	return GetHashValue(desc);
}

D3D12_SHADER_RESOURCE_VIEW_DESC CBSRUADescriptorHeap::GetShaderResourceViewDesc(DeviceBuffer* pBuffer)
{
	D3D12_SHADER_RESOURCE_VIEW_DESC desc = {};
	if (EnumMaskBitSet(pBuffer->GetBindFlags(), DeviceResource::BindFlags::AccelerationStructure))
	{
		desc.Format = DXGI_FORMAT_UNKNOWN;
		desc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
		desc.ViewDimension = D3D12_SRV_DIMENSION_RAYTRACING_ACCELERATION_STRUCTURE;
		desc.RaytracingAccelerationStructure.Location = pBuffer->GetGpuVirtualAddress();
	}
	else
	{
		desc.Format = DXGI_FORMAT_UNKNOWN;
		desc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
		desc.ViewDimension = D3D12_SRV_DIMENSION_BUFFER;
		desc.Buffer.FirstElement = 0;
		desc.Buffer.NumElements = pBuffer->GetMemoryRequested() / pBuffer->GetStride();
		desc.Buffer.StructureByteStride = pBuffer->GetStride();
		desc.Buffer.Flags = D3D12_BUFFER_SRV_FLAG_NONE;
	}
	return desc;
}

D3D12_SHADER_RESOURCE_VIEW_DESC CBSRUADescriptorHeap::GetShaderResourceViewDesc(DeviceTexture* pTexture, std::optional<UINT> MostDetailedMip, std::optional<UINT> MipLevels)
{
	D3D12_SHADER_RESOURCE_VIEW_DESC desc = {};
	UINT mostDetailedMip = MostDetailedMip.value_or(0);
	UINT mipLevels = MipLevels.value_or(pTexture->GetMipLevels());

	auto GetValidSRVFormat = [](DXGI_FORMAT Format)
	{
		switch (Format)
		{
		case DXGI_FORMAT_R32_TYPELESS:		return DXGI_FORMAT_R32_FLOAT;
		case DXGI_FORMAT_D24_UNORM_S8_UINT: return DXGI_FORMAT_R24_UNORM_X8_TYPELESS;

		default:
			return Format;
		}
	};

	desc.Format = GetValidSRVFormat(pTexture->GetFormat());
	desc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
	switch (pTexture->GetType())
	{
	case DeviceResource::Type::Texture1D:
	{
		if (pTexture->GetDepthOrArraySize() > 1)
		{
			desc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE1DARRAY;
			desc.Texture1DArray.MostDetailedMip = mostDetailedMip;
			desc.Texture1DArray.MipLevels = mipLevels;
			desc.Texture1DArray.FirstArraySlice = 0;
			desc.Texture1DArray.ArraySize = pTexture->GetDepthOrArraySize();
			desc.Texture1DArray.ResourceMinLODClamp = 0.0f;
		}
		else
		{
			desc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE1D;
			desc.Texture1D.MostDetailedMip = mostDetailedMip;
			desc.Texture1D.MipLevels = mipLevels;
			desc.Texture1D.ResourceMinLODClamp = 0.0f;
		}
	}
	break;

	case DeviceResource::Type::Texture2D:
	{
		if (pTexture->GetDepthOrArraySize() > 1)
		{
			desc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2DARRAY;
			desc.Texture2DArray.MostDetailedMip = mostDetailedMip;
			desc.Texture2DArray.MipLevels = mipLevels;
			desc.Texture2DArray.ArraySize = pTexture->GetDepthOrArraySize();
			desc.Texture2DArray.PlaneSlice = 0;
			desc.Texture2DArray.ResourceMinLODClamp = 0.0f;
		}
		else
		{
			desc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D;
			desc.Texture2D.MostDetailedMip = mostDetailedMip;
			desc.Texture2D.MipLevels = mipLevels;
			desc.Texture2D.PlaneSlice = 0;
			desc.Texture2D.ResourceMinLODClamp = 0.0f;
		}
	}
	break;

	case DeviceResource::Type::Texture3D:
	{
		desc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE3D;
		desc.Texture3D.MostDetailedMip = mostDetailedMip;
		desc.Texture3D.MipLevels = mipLevels;
		desc.Texture3D.ResourceMinLODClamp = 0.0f;
	}
	break;

	case DeviceResource::Type::TextureCube:
	{
		desc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURECUBE;
		desc.TextureCube.MostDetailedMip = mostDetailedMip;
		desc.TextureCube.MipLevels = mipLevels;
		desc.TextureCube.ResourceMinLODClamp = 0.0f;
	}
	break;
	}
	return desc;
}

D3D12_UNORDERED_ACCESS_VIEW_DESC CBSRUADescriptorHeap::GetUnorderedAccessViewDesc(DeviceTexture* pTexture, std::optional<UINT> ArraySlice, std::optional<UINT> MipSlice)
{
	D3D12_UNORDERED_ACCESS_VIEW_DESC desc = {};
	UINT arraySlice = ArraySlice.value_or(0);
	UINT mipSlice = MipSlice.value_or(0);
	desc.Format = pTexture->GetFormat();

	// TODO: Add buffer support
	switch (pTexture->GetType())
	{
	case DeviceResource::Type::Texture1D:
	{
		if (pTexture->GetDepthOrArraySize() > 1)
		{
			desc.ViewDimension = D3D12_UAV_DIMENSION_TEXTURE1DARRAY;
			desc.Texture1DArray.MipSlice = mipSlice;
			desc.Texture1DArray.FirstArraySlice = arraySlice;
			desc.Texture1DArray.ArraySize = pTexture->GetDepthOrArraySize();
		}
		else
		{
			desc.ViewDimension = D3D12_UAV_DIMENSION_TEXTURE1D;
			desc.Texture1D.MipSlice = mipSlice;
		}
	}
	break;

	case DeviceResource::Type::Texture2D: [[fallthrough]];
	case DeviceResource::Type::TextureCube:
	{
		if (pTexture->GetDepthOrArraySize() > 1)
		{
			desc.ViewDimension = D3D12_UAV_DIMENSION_TEXTURE2DARRAY;
			desc.Texture2DArray.MipSlice = mipSlice;
			desc.Texture2DArray.FirstArraySlice = arraySlice;
			desc.Texture2DArray.ArraySize = pTexture->GetDepthOrArraySize();
			desc.Texture2DArray.PlaneSlice = 0;
		}
		else
		{
			desc.ViewDimension = D3D12_UAV_DIMENSION_TEXTURE2D;
			desc.Texture2D.MipSlice = mipSlice;
			desc.Texture2D.PlaneSlice = 0;
		}
	}
	break;

	case DeviceResource::Type::Texture3D:
	{
		desc.ViewDimension = D3D12_UAV_DIMENSION_TEXTURE3D;
		desc.Texture3D.MipSlice = mipSlice;
		desc.Texture3D.FirstWSlice = 0;
		desc.Texture3D.WSize = 0;
	}
	break;
	}
	return desc;
}

UINT64 CBSRUADescriptorHeap::GetHashValue(const D3D12_SHADER_RESOURCE_VIEW_DESC& Desc)
{
	return std::hash<D3D12_SHADER_RESOURCE_VIEW_DESC>{}(Desc);
}

UINT64 CBSRUADescriptorHeap::GetHashValue(const D3D12_UNORDERED_ACCESS_VIEW_DESC& Desc)
{
	return std::hash<D3D12_UNORDERED_ACCESS_VIEW_DESC>{}(Desc);
}

//----------------------------------------------------------------------------------------------------
SamplerDescriptorHeap::SamplerDescriptorHeap(Device* pDevice, UINT NumDescriptors, bool ShaderVisible)
	: DescriptorHeap(pDevice, { NumDescriptors }, ShaderVisible, D3D12_DESCRIPTOR_HEAP_TYPE_SAMPLER)
{
}

//----------------------------------------------------------------------------------------------------
RenderTargetDescriptorHeap::RenderTargetDescriptorHeap(Device* pDevice, UINT NumDescriptors)
	: DescriptorHeap(pDevice, { NumDescriptors }, false, D3D12_DESCRIPTOR_HEAP_TYPE_RTV)
{
}

D3D12_RENDER_TARGET_VIEW_DESC RenderTargetDescriptorHeap::GetRenderTargetViewDesc(DeviceTexture* pTexture, std::optional<UINT> ArraySlice, std::optional<UINT> MipSlice, std::optional<UINT> ArraySize)
{
	D3D12_RENDER_TARGET_VIEW_DESC desc = {};
	UINT arraySlice = ArraySlice.value_or(0);
	UINT mipSlice = MipSlice.value_or(0);
	UINT arraySize = ArraySize.value_or(pTexture->GetDepthOrArraySize());

	desc.Format = pTexture->GetFormat();

	// TODO: Add buffer support and MS support
	switch (pTexture->GetType())
	{
	case DeviceResource::Type::Texture1D:
	{
		if (pTexture->GetDepthOrArraySize() > 1)
		{
			desc.ViewDimension = D3D12_RTV_DIMENSION_TEXTURE1DARRAY;
			desc.Texture1DArray.MipSlice = mipSlice;
			desc.Texture1DArray.FirstArraySlice = arraySlice;
			desc.Texture1DArray.ArraySize = arraySize;
		}
		else
		{
			desc.ViewDimension = D3D12_RTV_DIMENSION_TEXTURE1D;
			desc.Texture1D.MipSlice = mipSlice;
		}
	}
	break;

	case DeviceResource::Type::Texture2D: [[fallthrough]];
	case DeviceResource::Type::TextureCube:
	{
		if (pTexture->GetDepthOrArraySize() > 1)
		{
			desc.ViewDimension = D3D12_RTV_DIMENSION_TEXTURE2DARRAY;
			desc.Texture2DArray.MipSlice = mipSlice;
			desc.Texture2DArray.FirstArraySlice = arraySlice;
			desc.Texture2DArray.ArraySize = arraySize;
			desc.Texture2DArray.PlaneSlice = 0;
		}
		else
		{
			desc.ViewDimension = D3D12_RTV_DIMENSION_TEXTURE2D;
			desc.Texture2D.MipSlice = mipSlice;
			desc.Texture2D.PlaneSlice = 0;
		}
	}
	break;

	case DeviceResource::Type::Texture3D:
	{
		desc.ViewDimension = D3D12_RTV_DIMENSION_TEXTURE3D;
		desc.Texture3D.MipSlice = mipSlice;
		desc.Texture3D.FirstWSlice = 0;
		desc.Texture3D.WSize = 0;
	}
	break;
	}
	return desc;
}

UINT64 RenderTargetDescriptorHeap::GetHashValue(const D3D12_RENDER_TARGET_VIEW_DESC& Desc)
{
	return std::hash<D3D12_RENDER_TARGET_VIEW_DESC>{}(Desc);
}

void RenderTargetDescriptorHeap::Entry::operator=(DeviceTexture* pTexture)
{
	D3D12_RENDER_TARGET_VIEW_DESC desc = RenderTargetDescriptorHeap::GetRenderTargetViewDesc(pTexture, ArraySlice, MipSlice, ArraySize);
	pDevice->GetD3DDevice()->CreateRenderTargetView(pTexture->GetD3DResource(), &desc, DestDescriptor.CPUHandle);
	HashValue = RenderTargetDescriptorHeap::GetHashValue(desc);
}

//----------------------------------------------------------------------------------------------------
DepthStencilDescriptorHeap::DepthStencilDescriptorHeap(Device* pDevice, UINT NumDescriptors)
	: DescriptorHeap(pDevice, { NumDescriptors }, false, D3D12_DESCRIPTOR_HEAP_TYPE_DSV)
{
}

D3D12_DEPTH_STENCIL_VIEW_DESC DepthStencilDescriptorHeap::GetDepthStencilViewDesc(DeviceTexture* pTexture, std::optional<UINT> ArraySlice, std::optional<UINT> MipSlice, std::optional<UINT> ArraySize)
{
	D3D12_DEPTH_STENCIL_VIEW_DESC desc = {};
	UINT arraySlice = ArraySlice.value_or(0);
	UINT mipSlice = MipSlice.value_or(0);
	UINT arraySize = ArraySize.value_or(pTexture->GetDepthOrArraySize());

	auto getValidDSVFormat = [](DXGI_FORMAT Format)
	{
		// TODO: Add more
		switch (Format)
		{
		case DXGI_FORMAT_R32_TYPELESS: return DXGI_FORMAT_D32_FLOAT;

		default: return DXGI_FORMAT_UNKNOWN;
		}
	};

	desc.Format = getValidDSVFormat(pTexture->GetFormat());
	desc.Flags = D3D12_DSV_FLAG_NONE;

	// TODO: Add support and MS support
	switch (pTexture->GetType())
	{
	case DeviceResource::Type::Texture1D:
	{
		if (pTexture->GetDepthOrArraySize() > 1)
		{
			desc.ViewDimension = D3D12_DSV_DIMENSION_TEXTURE1DARRAY;
			desc.Texture1DArray.MipSlice = mipSlice;
			desc.Texture1DArray.FirstArraySlice = arraySlice;
			desc.Texture1DArray.ArraySize = arraySize;
		}
		else
		{
			desc.ViewDimension = D3D12_DSV_DIMENSION_TEXTURE1D;
			desc.Texture1D.MipSlice = mipSlice;
		}
	}
	break;

	case DeviceResource::Type::Texture2D: [[fallthrough]];
	case DeviceResource::Type::TextureCube:
	{
		if (pTexture->GetDepthOrArraySize() > 1)
		{
			desc.ViewDimension = D3D12_DSV_DIMENSION_TEXTURE2DARRAY;
			desc.Texture2DArray.MipSlice = mipSlice;
			desc.Texture2DArray.FirstArraySlice = arraySlice;
			desc.Texture2DArray.ArraySize = arraySize;
		}
		else
		{
			desc.ViewDimension = D3D12_DSV_DIMENSION_TEXTURE2D;
			desc.Texture2D.MipSlice = mipSlice;
		}
	}
	break;

	case DeviceResource::Type::Texture3D:
	{
		throw std::logic_error("Cannot create DSV for Texture3D resource type");
	}
	break;
	}
	return desc;
}

UINT64 DepthStencilDescriptorHeap::GetHashValue(const D3D12_DEPTH_STENCIL_VIEW_DESC& Desc)
{
	return std::hash<D3D12_DEPTH_STENCIL_VIEW_DESC>{}(Desc);
}

void DepthStencilDescriptorHeap::Entry::operator=(DeviceTexture* pTexture)
{
	D3D12_DEPTH_STENCIL_VIEW_DESC desc = DepthStencilDescriptorHeap::GetDepthStencilViewDesc(pTexture, ArraySlice, MipSlice, ArraySize);
	pDevice->GetD3DDevice()->CreateDepthStencilView(pTexture->GetD3DResource(), &desc, DestDescriptor.CPUHandle);
	HashValue = DepthStencilDescriptorHeap::GetHashValue(desc);
}