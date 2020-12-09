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

	D3D12_DESCRIPTOR_HEAP_DESC Desc = 
	{
		.Type			= Type,
		.NumDescriptors = std::accumulate(Ranges.begin(), Ranges.end(), 0u),
		.Flags			= m_ShaderVisible ? D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE : D3D12_DESCRIPTOR_HEAP_FLAG_NONE,
		.NodeMask		= 0
	};

	ThrowCOMIfFailed(pDevice->GetApiHandle()->CreateDescriptorHeap(&Desc, IID_PPV_ARGS(&m_pDescriptorHeap)));
	m_DescriptorIncrementSize = pDevice->GetDescriptorIncrementSize(Type);

	D3D12_CPU_DESCRIPTOR_HANDLE StartCpuHandle = m_pDescriptorHeap->GetCPUDescriptorHandleForHeapStart();
	D3D12_GPU_DESCRIPTOR_HANDLE StartGpuHandle = {};
	if (m_ShaderVisible)
		StartGpuHandle = m_pDescriptorHeap->GetGPUDescriptorHandleForHeapStart();
	m_StartDescriptor = { StartCpuHandle, StartGpuHandle, 0 };

	UINT HeapIndex = 0;
	m_DescriptorPartitions.resize(Ranges.size());
	for (std::size_t i = 0; i < Ranges.size(); ++i)
	{
		D3D12_CPU_DESCRIPTOR_HANDLE CpuHandle = {};
		D3D12_GPU_DESCRIPTOR_HANDLE GpuHandle = {};
		CD3DX12_CPU_DESCRIPTOR_HANDLE::InitOffsetted(CpuHandle, StartCpuHandle, HeapIndex, m_DescriptorIncrementSize);
		CD3DX12_GPU_DESCRIPTOR_HANDLE::InitOffsetted(GpuHandle, StartGpuHandle, HeapIndex, m_DescriptorIncrementSize);

		m_DescriptorPartitions[i].StartDescriptor = { CpuHandle, GpuHandle, HeapIndex };
		m_DescriptorPartitions[i].NumDescriptors = Ranges[i];

		HeapIndex += Ranges[i];
	}
}

Descriptor DescriptorHeap::GetDescriptorAt(INT PartitionIndex, UINT HeapIndex) const
{
	const auto& partition = GetDescriptorPartitionAt(PartitionIndex);
	D3D12_CPU_DESCRIPTOR_HANDLE CpuHandle = {};
	D3D12_GPU_DESCRIPTOR_HANDLE GpuHandle = {};
	CD3DX12_CPU_DESCRIPTOR_HANDLE::InitOffsetted(CpuHandle, partition.StartDescriptor.CpuHandle, HeapIndex, m_DescriptorIncrementSize);
	CD3DX12_GPU_DESCRIPTOR_HANDLE::InitOffsetted(GpuHandle, partition.StartDescriptor.GpuHandle, HeapIndex, m_DescriptorIncrementSize);

	return { CpuHandle, GpuHandle, HeapIndex };
}

//----------------------------------------------------------------------------------------------------
CBSRUADescriptorHeap::CBSRUADescriptorHeap(Device* pDevice, UINT NumCBDescriptors, UINT NumSRDescriptors, UINT NumUADescriptors, bool ShaderVisible)
	: DescriptorHeap(pDevice, { NumCBDescriptors, NumSRDescriptors, NumUADescriptors }, ShaderVisible, D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV)
{
}

UINT64 CBSRUADescriptorHeap::AssignSRDescriptor(UINT HeapIndex, Buffer* pBuffer)
{
	auto DestDescriptor = GetDescriptorAt(ShaderResource, HeapIndex);
	auto Desc			= GetShaderResourceViewDesc(pBuffer);
	if (EnumMaskBitSet(pBuffer->GetBindFlags(), Resource::Flags::AccelerationStructure))
	{
		m_pDevice->GetApiHandle()->CreateShaderResourceView(nullptr, &Desc, DestDescriptor.CpuHandle);
	}
	else
	{
		m_pDevice->GetApiHandle()->CreateShaderResourceView(pBuffer->GetApiHandle(), &Desc, DestDescriptor.CpuHandle);
	}
	return GetHashValue(Desc);
}

UINT64 CBSRUADescriptorHeap::AssignSRDescriptor(UINT HeapIndex, Texture* pTexture, std::optional<UINT> MostDetailedMip, std::optional<UINT> MipLevels)
{
	auto DestDescriptor = GetDescriptorAt(ShaderResource, HeapIndex);
	auto Desc			= GetShaderResourceViewDesc(pTexture, MostDetailedMip, MipLevels);
	m_pDevice->GetApiHandle()->CreateShaderResourceView(pTexture->GetApiHandle(), &Desc, DestDescriptor.CpuHandle);
	return GetHashValue(Desc);
}

UINT64 CBSRUADescriptorHeap::AssignUADescriptor(UINT HeapIndex, Texture* pTexture, std::optional<UINT> ArraySlice, std::optional<UINT> MipSlice)
{
	auto DestDescriptor = GetDescriptorAt(UnorderedAccess, HeapIndex);
	auto Desc			= GetUnorderedAccessViewDesc(pTexture, ArraySlice, MipSlice);
	m_pDevice->GetApiHandle()->CreateUnorderedAccessView(pTexture->GetApiHandle(), nullptr, &Desc, DestDescriptor.CpuHandle);
	return GetHashValue(Desc);
}

D3D12_SHADER_RESOURCE_VIEW_DESC CBSRUADescriptorHeap::GetShaderResourceViewDesc(Buffer* pBuffer)
{
	D3D12_SHADER_RESOURCE_VIEW_DESC Desc				= {};
	if (EnumMaskBitSet(pBuffer->GetBindFlags(), Resource::Flags::AccelerationStructure))
	{
		Desc.Format										= DXGI_FORMAT_UNKNOWN;
		Desc.Shader4ComponentMapping					= D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
		Desc.ViewDimension								= D3D12_SRV_DIMENSION_RAYTRACING_ACCELERATION_STRUCTURE;
		Desc.RaytracingAccelerationStructure.Location	= pBuffer->GetGpuVirtualAddress();
	}
	else
	{
		Desc.Format										= DXGI_FORMAT_UNKNOWN;
		Desc.Shader4ComponentMapping					= D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
		Desc.ViewDimension								= D3D12_SRV_DIMENSION_BUFFER;
		Desc.Buffer.FirstElement						= 0;
		Desc.Buffer.NumElements							= pBuffer->GetMemoryRequested() / pBuffer->GetStride();
		Desc.Buffer.StructureByteStride					= pBuffer->GetStride();
		Desc.Buffer.Flags								= D3D12_BUFFER_SRV_FLAG_NONE;
	}
	return Desc;
}

D3D12_SHADER_RESOURCE_VIEW_DESC CBSRUADescriptorHeap::GetShaderResourceViewDesc(Texture* pTexture, std::optional<UINT> MostDetailedMip, std::optional<UINT> MipLevels)
{
	D3D12_SHADER_RESOURCE_VIEW_DESC Desc = {};
	UINT mostDetailedMip = MostDetailedMip.value_or(0);
	UINT mipLevels = MipLevels.value_or(pTexture->GetMipLevels());

	auto GetValidSRVFormat = [](DXGI_FORMAT Format)
	{
		switch (Format)
		{
		case DXGI_FORMAT_R32_TYPELESS:		return DXGI_FORMAT_R32_FLOAT;
		case DXGI_FORMAT_D24_UNORM_S8_UINT: return DXGI_FORMAT_R24_UNORM_X8_TYPELESS;
		case DXGI_FORMAT_D32_FLOAT:			return DXGI_FORMAT_R32_FLOAT;
		default:							return Format;
		}
	};

	Desc.Format = GetValidSRVFormat(pTexture->GetFormat());
	Desc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;

	switch (pTexture->GetType())
	{
	case Resource::Type::Texture1D:
	{
		if (pTexture->GetDepthOrArraySize() > 1)
		{
			Desc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE1DARRAY;
			Desc.Texture1DArray.MostDetailedMip = mostDetailedMip;
			Desc.Texture1DArray.MipLevels = mipLevels;
			Desc.Texture1DArray.FirstArraySlice = 0;
			Desc.Texture1DArray.ArraySize = pTexture->GetDepthOrArraySize();
			Desc.Texture1DArray.ResourceMinLODClamp = 0.0f;
		}
		else
		{
			Desc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE1D;
			Desc.Texture1D.MostDetailedMip = mostDetailedMip;
			Desc.Texture1D.MipLevels = mipLevels;
			Desc.Texture1D.ResourceMinLODClamp = 0.0f;
		}
	}
	break;

	case Resource::Type::Texture2D:
	{
		if (pTexture->GetDepthOrArraySize() > 1)
		{
			Desc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2DARRAY;
			Desc.Texture2DArray.MostDetailedMip = mostDetailedMip;
			Desc.Texture2DArray.MipLevels = mipLevels;
			Desc.Texture2DArray.ArraySize = pTexture->GetDepthOrArraySize();
			Desc.Texture2DArray.PlaneSlice = 0;
			Desc.Texture2DArray.ResourceMinLODClamp = 0.0f;
		}
		else
		{
			Desc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D;
			Desc.Texture2D.MostDetailedMip = mostDetailedMip;
			Desc.Texture2D.MipLevels = mipLevels;
			Desc.Texture2D.PlaneSlice = 0;
			Desc.Texture2D.ResourceMinLODClamp = 0.0f;
		}
	}
	break;

	case Resource::Type::Texture3D:
	{
		Desc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE3D;
		Desc.Texture3D.MostDetailedMip = mostDetailedMip;
		Desc.Texture3D.MipLevels = mipLevels;
		Desc.Texture3D.ResourceMinLODClamp = 0.0f;
	}
	break;

	case Resource::Type::TextureCube:
	{
		Desc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURECUBE;
		Desc.TextureCube.MostDetailedMip = mostDetailedMip;
		Desc.TextureCube.MipLevels = mipLevels;
		Desc.TextureCube.ResourceMinLODClamp = 0.0f;
	}
	break;
	}
	return Desc;
}

D3D12_UNORDERED_ACCESS_VIEW_DESC CBSRUADescriptorHeap::GetUnorderedAccessViewDesc(Texture* pTexture, std::optional<UINT> ArraySlice, std::optional<UINT> MipSlice)
{
	D3D12_UNORDERED_ACCESS_VIEW_DESC Desc = {};
	UINT arraySlice = ArraySlice.value_or(0);
	UINT mipSlice = MipSlice.value_or(0);

	Desc.Format = pTexture->GetFormat();

	// TODO: Add buffer support
	switch (pTexture->GetType())
	{
	case Resource::Type::Texture1D:
	{
		if (pTexture->GetDepthOrArraySize() > 1)
		{
			Desc.ViewDimension = D3D12_UAV_DIMENSION_TEXTURE1DARRAY;
			Desc.Texture1DArray.MipSlice = mipSlice;
			Desc.Texture1DArray.FirstArraySlice = arraySlice;
			Desc.Texture1DArray.ArraySize = pTexture->GetDepthOrArraySize();
		}
		else
		{
			Desc.ViewDimension = D3D12_UAV_DIMENSION_TEXTURE1D;
			Desc.Texture1D.MipSlice = mipSlice;
		}
	}
	break;

	case Resource::Type::Texture2D: [[fallthrough]];
	case Resource::Type::TextureCube:
	{
		if (pTexture->GetDepthOrArraySize() > 1)
		{
			Desc.ViewDimension = D3D12_UAV_DIMENSION_TEXTURE2DARRAY;
			Desc.Texture2DArray.MipSlice = mipSlice;
			Desc.Texture2DArray.FirstArraySlice = arraySlice;
			Desc.Texture2DArray.ArraySize = pTexture->GetDepthOrArraySize();
			Desc.Texture2DArray.PlaneSlice = 0;
		}
		else
		{
			Desc.ViewDimension = D3D12_UAV_DIMENSION_TEXTURE2D;
			Desc.Texture2D.MipSlice = mipSlice;
			Desc.Texture2D.PlaneSlice = 0;
		}
	}
	break;

	case Resource::Type::Texture3D:
	{
		Desc.ViewDimension = D3D12_UAV_DIMENSION_TEXTURE3D;
		Desc.Texture3D.MipSlice = mipSlice;
		Desc.Texture3D.FirstWSlice = 0;
		Desc.Texture3D.WSize = 0;
	}
	break;
	}
	return Desc;
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

D3D12_RENDER_TARGET_VIEW_DESC RenderTargetDescriptorHeap::GetRenderTargetViewDesc(Texture* pTexture, std::optional<UINT> ArraySlice, std::optional<UINT> MipSlice, std::optional<UINT> ArraySize)
{
	D3D12_RENDER_TARGET_VIEW_DESC Desc = {};
	UINT arraySlice = ArraySlice.value_or(0);
	UINT mipSlice = MipSlice.value_or(0);
	UINT arraySize = ArraySize.value_or(pTexture->GetDepthOrArraySize());

	Desc.Format = pTexture->GetFormat();

	// TODO: Add buffer support and MS support
	switch (pTexture->GetType())
	{
	case Resource::Type::Texture1D:
	{
		if (pTexture->GetDepthOrArraySize() > 1)
		{
			Desc.ViewDimension = D3D12_RTV_DIMENSION_TEXTURE1DARRAY;
			Desc.Texture1DArray.MipSlice = mipSlice;
			Desc.Texture1DArray.FirstArraySlice = arraySlice;
			Desc.Texture1DArray.ArraySize = arraySize;
		}
		else
		{
			Desc.ViewDimension = D3D12_RTV_DIMENSION_TEXTURE1D;
			Desc.Texture1D.MipSlice = mipSlice;
		}
	}
	break;

	case Resource::Type::Texture2D: [[fallthrough]];
	case Resource::Type::TextureCube:
	{
		if (pTexture->GetDepthOrArraySize() > 1)
		{
			Desc.ViewDimension = D3D12_RTV_DIMENSION_TEXTURE2DARRAY;
			Desc.Texture2DArray.MipSlice = mipSlice;
			Desc.Texture2DArray.FirstArraySlice = arraySlice;
			Desc.Texture2DArray.ArraySize = arraySize;
			Desc.Texture2DArray.PlaneSlice = 0;
		}
		else
		{
			Desc.ViewDimension = D3D12_RTV_DIMENSION_TEXTURE2D;
			Desc.Texture2D.MipSlice = mipSlice;
			Desc.Texture2D.PlaneSlice = 0;
		}
	}
	break;

	case Resource::Type::Texture3D:
	{
		Desc.ViewDimension = D3D12_RTV_DIMENSION_TEXTURE3D;
		Desc.Texture3D.MipSlice = mipSlice;
		Desc.Texture3D.FirstWSlice = 0;
		Desc.Texture3D.WSize = 0;
	}
	break;
	}
	return Desc;
}

UINT64 RenderTargetDescriptorHeap::GetHashValue(const D3D12_RENDER_TARGET_VIEW_DESC& Desc)
{
	return std::hash<D3D12_RENDER_TARGET_VIEW_DESC>{}(Desc);
}

void RenderTargetDescriptorHeap::Entry::operator=(Texture* pTexture)
{
	auto Desc = RenderTargetDescriptorHeap::GetRenderTargetViewDesc(pTexture, ArraySlice, MipSlice, ArraySize);
	HashValue = RenderTargetDescriptorHeap::GetHashValue(Desc);
	pDevice->GetApiHandle()->CreateRenderTargetView(pTexture->GetApiHandle(), &Desc, DestDescriptor.CpuHandle);
}

//----------------------------------------------------------------------------------------------------
DepthStencilDescriptorHeap::DepthStencilDescriptorHeap(Device* pDevice, UINT NumDescriptors)
	: DescriptorHeap(pDevice, { NumDescriptors }, false, D3D12_DESCRIPTOR_HEAP_TYPE_DSV)
{
}

D3D12_DEPTH_STENCIL_VIEW_DESC DepthStencilDescriptorHeap::GetDepthStencilViewDesc(Texture* pTexture, std::optional<UINT> ArraySlice, std::optional<UINT> MipSlice, std::optional<UINT> ArraySize)
{
	D3D12_DEPTH_STENCIL_VIEW_DESC Desc = {};
	UINT arraySlice = ArraySlice.value_or(0);
	UINT mipSlice = MipSlice.value_or(0);
	UINT arraySize = ArraySize.value_or(pTexture->GetDepthOrArraySize());

	auto GetValidDSVFormat = [](DXGI_FORMAT Format)
	{
		// TODO: Add more
		switch (Format)
		{
		case DXGI_FORMAT_R32_TYPELESS: return DXGI_FORMAT_D32_FLOAT;

		default: return DXGI_FORMAT_UNKNOWN;
		}
	};

	Desc.Format = GetValidDSVFormat(pTexture->GetFormat());
	Desc.Flags = D3D12_DSV_FLAG_NONE;

	// TODO: Add support and MS support
	switch (pTexture->GetType())
	{
	case Resource::Type::Texture1D:
	{
		if (pTexture->GetDepthOrArraySize() > 1)
		{
			Desc.ViewDimension = D3D12_DSV_DIMENSION_TEXTURE1DARRAY;
			Desc.Texture1DArray.MipSlice = mipSlice;
			Desc.Texture1DArray.FirstArraySlice = arraySlice;
			Desc.Texture1DArray.ArraySize = arraySize;
		}
		else
		{
			Desc.ViewDimension = D3D12_DSV_DIMENSION_TEXTURE1D;
			Desc.Texture1D.MipSlice = mipSlice;
		}
	}
	break;

	case Resource::Type::Texture2D: [[fallthrough]];
	case Resource::Type::TextureCube:
	{
		if (pTexture->GetDepthOrArraySize() > 1)
		{
			Desc.ViewDimension = D3D12_DSV_DIMENSION_TEXTURE2DARRAY;
			Desc.Texture2DArray.MipSlice = mipSlice;
			Desc.Texture2DArray.FirstArraySlice = arraySlice;
			Desc.Texture2DArray.ArraySize = arraySize;
		}
		else
		{
			Desc.ViewDimension = D3D12_DSV_DIMENSION_TEXTURE2D;
			Desc.Texture2D.MipSlice = mipSlice;
		}
	}
	break;

	case Resource::Type::Texture3D:
	{
		throw std::logic_error("Cannot create DSV for Texture3D resource type");
	}
	break;
	}
	return Desc;
}

UINT64 DepthStencilDescriptorHeap::GetHashValue(const D3D12_DEPTH_STENCIL_VIEW_DESC& Desc)
{
	return std::hash<D3D12_DEPTH_STENCIL_VIEW_DESC>{}(Desc);
}

void DepthStencilDescriptorHeap::Entry::operator=(Texture* pTexture)
{
	auto Desc = DepthStencilDescriptorHeap::GetDepthStencilViewDesc(pTexture, ArraySlice, MipSlice, ArraySize);
	HashValue = DepthStencilDescriptorHeap::GetHashValue(Desc);
	pDevice->GetApiHandle()->CreateDepthStencilView(pTexture->GetApiHandle(), &Desc, DestDescriptor.CpuHandle);
}