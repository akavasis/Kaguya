#include "pch.h"
#include "GpuTextureAllocator.h"
#include "RendererRegistry.h"

GpuTextureAllocator::GpuTextureAllocator(RenderDevice* SV_pRenderDevice)
	: SV_pRenderDevice(SV_pRenderDevice)
{
	DXGI_FORMAT OptionalFormats[] =
	{
		DXGI_FORMAT_R16G16B16A16_UNORM,
		DXGI_FORMAT_R16G16B16A16_SNORM,
		DXGI_FORMAT_R32G32_FLOAT,
		DXGI_FORMAT_R32G32_UINT,
		DXGI_FORMAT_R32G32_SINT,
		DXGI_FORMAT_R10G10B10A2_UNORM,
		DXGI_FORMAT_R10G10B10A2_UINT,
		DXGI_FORMAT_R11G11B10_FLOAT,
		DXGI_FORMAT_R8G8B8A8_SNORM,
		DXGI_FORMAT_R16G16_FLOAT,
		DXGI_FORMAT_R16G16_UNORM,
		DXGI_FORMAT_R16G16_UINT,
		DXGI_FORMAT_R16G16_SNORM,
		DXGI_FORMAT_R16G16_SINT,
		DXGI_FORMAT_R8G8_UNORM,
		DXGI_FORMAT_R8G8_UINT,
		DXGI_FORMAT_R8G8_SNORM,
		DXGI_FORMAT_R8G8_SINT,
		DXGI_FORMAT_R16_UNORM,
		DXGI_FORMAT_R16_SNORM,
		DXGI_FORMAT_R8_SNORM,
		DXGI_FORMAT_A8_UNORM,
		DXGI_FORMAT_B5G6R5_UNORM,
		DXGI_FORMAT_B5G5R5A1_UNORM,
		DXGI_FORMAT_B4G4R4A4_UNORM
	};
	const int NumOptionalFormats = ARRAYSIZE(OptionalFormats);
	for (int i = 0; i < NumOptionalFormats; ++i)
	{
		D3D12_FEATURE_DATA_FORMAT_SUPPORT FeatureDataFormatSupport	= {};
		FeatureDataFormatSupport.Format								= OptionalFormats[i];

		ThrowCOMIfFailed(SV_pRenderDevice->Device.GetD3DDevice()->CheckFeatureSupport(
			D3D12_FEATURE_FORMAT_SUPPORT,
			&FeatureDataFormatSupport,
			sizeof(D3D12_FEATURE_DATA_FORMAT_SUPPORT)));

		auto CheckFormatSupport1 = [&](D3D12_FORMAT_SUPPORT1 FormatSupport)
		{
			return (FeatureDataFormatSupport.Support1 & FormatSupport) != 0;
		};
		auto CheckFormatSupport2 = [&](D3D12_FORMAT_SUPPORT2 FormatSupport)
		{
			return (FeatureDataFormatSupport.Support2 & FormatSupport) != 0;
		};

		bool SupportUAV = CheckFormatSupport1(D3D12_FORMAT_SUPPORT1_TYPED_UNORDERED_ACCESS_VIEW) &&
			CheckFormatSupport2(D3D12_FORMAT_SUPPORT2_UAV_TYPED_LOAD) &&
			CheckFormatSupport2(D3D12_FORMAT_SUPPORT2_UAV_TYPED_STORE);

		if (SupportUAV)
		{
			m_UAVSupportedFormat.insert(OptionalFormats[i]);
		}
	}

	m_SystemReservedTextures[DefaultWhite]							= LoadFromFile(Application::ExecutableFolderPath / "Assets/Textures/DefaultWhite.dds", false, false);
	m_SystemReservedTextures[DefaultBlack]							= LoadFromFile(Application::ExecutableFolderPath / "Assets/Textures/DefaultBlack.dds", false, false);
	m_SystemReservedTextures[DefaultAlbedo]							= LoadFromFile(Application::ExecutableFolderPath / "Assets/Textures/DefaultAlbedoMap.dds", true, false);
	m_SystemReservedTextures[DefaultNormal]							= LoadFromFile(Application::ExecutableFolderPath / "Assets/Textures/DefaultNormalMap.dds", false, false);
	m_SystemReservedTextures[DefaultRoughness]						= LoadFromFile(Application::ExecutableFolderPath / "Assets/Textures/DefaultRoughnessMap.dds", false, false);
	m_SystemReservedTextures[BRDFLUT]								= LoadFromFile(Application::ExecutableFolderPath / "Assets/LUT/BRDF.dds", false, false);
	m_SystemReservedTextures[LTC_LUT_DisneyDiffuse_InverseMatrix]	= LoadFromFile(Application::ExecutableFolderPath / "Assets/LUT/LTC_LUT_DisneyDiffuse_InverseMatrix.dds", false, false);
	m_SystemReservedTextures[LTC_LUT_DisneyDiffuse_Terms]			= LoadFromFile(Application::ExecutableFolderPath / "Assets/LUT/LTC_LUT_DisneyDiffuse_Terms.dds", false, false);
	m_SystemReservedTextures[LTC_LUT_GGX_InverseMatrix]				= LoadFromFile(Application::ExecutableFolderPath / "Assets/LUT/LTC_LUT_GGX_InverseMatrix.dds", false, false);
	m_SystemReservedTextures[LTC_LUT_GGX_Terms]						= LoadFromFile(Application::ExecutableFolderPath / "Assets/LUT/LTC_LUT_GGX_Terms.dds", false, false);
	m_SystemReservedTextures[BlueNoise]								= LoadFromFile(Application::ExecutableFolderPath / "Assets/LUT/Blue_Noise_RGBA_0.dds", false, false);
}

void GpuTextureAllocator::StageSystemReservedTextures(RenderContext& RenderContext)
{
	for (auto& [handle, stagingTexture] : m_UnstagedTextures)
	{
		StageTexture(handle, stagingTexture, RenderContext);
	}
}

void GpuTextureAllocator::Stage(Scene& Scene, RenderContext& RenderContext)
{
	if (!Status::SkyboxGenerated && !Scene.Skybox.Path.empty())
	{
		Status::SkyboxGenerated = true;

		m_SystemReservedTextures[SkyboxEquirectangularMap] = LoadFromFile(Scene.Skybox.Path, false, true);
		auto& stagingTexture = m_UnstagedTextures[m_SystemReservedTextures[SkyboxEquirectangularMap]];
		StageTexture(m_SystemReservedTextures[SkyboxEquirectangularMap], stagingTexture, RenderContext);

		// Generate cubemap for equirectangular map
		DeviceTexture* pEquirectangularMap = SV_pRenderDevice->GetTexture(m_SystemReservedTextures[SkyboxEquirectangularMap]);
		m_SystemReservedTextures[SkyboxCubemap] = SV_pRenderDevice->CreateDeviceTexture(DeviceResource::Type::TextureCube, [&](DeviceTextureProxy& proxy)
		{
			proxy.SetFormat(pEquirectangularMap->GetFormat());
			proxy.SetWidth(1024);
			proxy.SetHeight(1024);
			proxy.SetMipLevels(0);
			proxy.BindFlags = DeviceResource::BindFlags::UnorderedAccess;
			proxy.InitialState = DeviceResource::State::Common;
		});

		EquirectangularToCubemap(m_SystemReservedTextures[SkyboxEquirectangularMap], m_SystemReservedTextures[SkyboxCubemap], RenderContext);
		SV_pRenderDevice->CreateShaderResourceView(m_SystemReservedTextures[SkyboxCubemap]);
	}

	for (auto& material : Scene.Materials)
	{
		LoadMaterial(material);
	}

	// Gpu copy
	for (auto& [handle, stagingTexture] : m_UnstagedTextures)
	{
		// Skybox is already staged, dont need to stage again
		if (handle == m_SystemReservedTextures[SkyboxEquirectangularMap])
			continue;

		StageTexture(handle, stagingTexture, RenderContext);
	}
}

void GpuTextureAllocator::DisposeResources()
{
	m_UnstagedTextures.clear();

	for (auto& handle : m_TemporaryResources)
	{
		SV_pRenderDevice->Destroy(&handle);
	}
	m_TemporaryResources.clear();
}

RenderResourceHandle GpuTextureAllocator::LoadFromFile(const std::filesystem::path& Path, bool ForceSRGB, bool GenerateMips)
{
	assert(std::filesystem::exists(Path) && "File not found");

	if (auto iter = m_Textures.find(Path.generic_string());
		iter != m_Textures.end())
	{
		return iter->second;
	}

	DirectX::ScratchImage	ScratchImage;
	DirectX::TexMetadata	TexMetadata;
	if (Path.extension() == ".dds")
	{
		ThrowCOMIfFailed(DirectX::LoadFromDDSFile(Path.c_str(), DirectX::DDS_FLAGS::DDS_FLAGS_FORCE_RGB, &TexMetadata, ScratchImage));
	}
	else if (Path.extension() == ".tga")
	{
		ThrowCOMIfFailed(DirectX::LoadFromTGAFile(Path.c_str(), &TexMetadata, ScratchImage));
	}
	else if (Path.extension() == ".hdr")
	{
		ThrowCOMIfFailed(DirectX::LoadFromHDRFile(Path.c_str(), &TexMetadata, ScratchImage));
	}
	else
	{
		ThrowCOMIfFailed(DirectX::LoadFromWICFile(Path.c_str(), DirectX::WIC_FLAGS::WIC_FLAGS_FORCE_RGB, &TexMetadata, ScratchImage));
	}
	// if metadata mip levels is > 1 then is already generated, other wise the value is based on argument
	GenerateMips = (TexMetadata.mipLevels > 1) ? false : GenerateMips;
	size_t MipLevels = GenerateMips ? static_cast<size_t>(std::floor(std::log2(std::max(TexMetadata.width, TexMetadata.height)))) + 1 : TexMetadata.mipLevels;

	if (ForceSRGB)
	{
		TexMetadata.format = DirectX::MakeSRGB(TexMetadata.format);
	}

	// Create actual texture
	RenderResourceHandle		TextureHandle;
	DeviceResource::BindFlags	BindFlags = GenerateMips ? DeviceResource::BindFlags::UnorderedAccess : DeviceResource::BindFlags::None;

	switch (TexMetadata.dimension)
	{
	case DirectX::TEX_DIMENSION::TEX_DIMENSION_TEXTURE1D:
	{
		TextureHandle = SV_pRenderDevice->CreateDeviceTexture(DeviceResource::Type::Texture1D, [&](DeviceTextureProxy& proxy)
		{
			proxy.SetFormat(TexMetadata.format);
			proxy.SetWidth(static_cast<UINT64>(TexMetadata.width));
			proxy.SetDepthOrArraySize(static_cast<UINT16>(TexMetadata.arraySize));
			proxy.SetMipLevels(MipLevels);
			proxy.BindFlags = BindFlags;
			proxy.InitialState = DeviceResource::State::CopyDest;
		});
	}
	break;

	case DirectX::TEX_DIMENSION::TEX_DIMENSION_TEXTURE2D:
	{
		TextureHandle = SV_pRenderDevice->CreateDeviceTexture(DeviceResource::Type::Texture2D, [&](DeviceTextureProxy& proxy)
		{
			proxy.SetFormat(TexMetadata.format);
			proxy.SetWidth(static_cast<UINT64>(TexMetadata.width));
			proxy.SetHeight(static_cast<UINT>(TexMetadata.height));
			proxy.SetDepthOrArraySize(static_cast<UINT16>(TexMetadata.arraySize));
			proxy.SetMipLevels(MipLevels);
			proxy.BindFlags = BindFlags;
			proxy.InitialState = DeviceResource::State::CopyDest;
		});
	}
	break;

	case DirectX::TEX_DIMENSION::TEX_DIMENSION_TEXTURE3D:
	{
		TextureHandle = SV_pRenderDevice->CreateDeviceTexture(DeviceResource::Type::Texture3D, [&](DeviceTextureProxy& proxy)
		{
			proxy.SetFormat(TexMetadata.format);
			proxy.SetWidth(static_cast<UINT64>(TexMetadata.width));
			proxy.SetHeight(static_cast<UINT>(TexMetadata.height));
			proxy.SetDepthOrArraySize(static_cast<UINT16>(TexMetadata.depth));
			proxy.SetMipLevels(MipLevels);
			proxy.BindFlags = BindFlags;
			proxy.InitialState = DeviceResource::State::CopyDest;
		});
	}
	break;

	default: assert(false && "Unknown DirectX::TEX_DIMENSION");
	}

	DeviceTexture* pTexture = SV_pRenderDevice->GetTexture(TextureHandle);
#ifdef _DEBUG
	pTexture->GetD3DResource()->SetName(Path.generic_wstring().data());
#endif

	std::vector<D3D12_SUBRESOURCE_DATA> subresources(ScratchImage.GetImageCount());
	const DirectX::Image* pImages = ScratchImage.GetImages();
	for (size_t i = 0; i < ScratchImage.GetImageCount(); ++i)
	{
		subresources[i].RowPitch = pImages[i].rowPitch;
		subresources[i].SlicePitch = pImages[i].slicePitch;
		subresources[i].pData = pImages[i].pixels;
	}

	Microsoft::WRL::ComPtr<ID3D12Resource>			StagingResource;
	const UINT										NumSubresources						= static_cast<UINT>(subresources.size());
	std::vector<D3D12_PLACED_SUBRESOURCE_FOOTPRINT> PlacedSubresourceLayouts			(NumSubresources);
	std::vector<UINT>								NumRows								(NumSubresources);
	std::vector<UINT64>								RowSizeInBytes						(NumSubresources);
	UINT64											TotalBytes							= 0;

	auto pD3DDevice = SV_pRenderDevice->Device.GetD3DDevice();
	auto Desc = pTexture->GetD3DResource()->GetDesc();
	pD3DDevice->GetCopyableFootprints(&Desc, 0, NumSubresources, 0,
		PlacedSubresourceLayouts.data(), NumRows.data(), RowSizeInBytes.data(), &TotalBytes);

	Desc = CD3DX12_RESOURCE_DESC::Buffer(TotalBytes);
	pD3DDevice->CreateCommittedResource(&kUploadHeapProps, D3D12_HEAP_FLAG_NONE,
		&Desc, D3D12_RESOURCE_STATE_GENERIC_READ, nullptr, IID_PPV_ARGS(StagingResource.ReleaseAndGetAddressOf()));

	// CPU Upload
	BYTE* pHostMemory = nullptr;
	if (StagingResource->Map(0, nullptr, reinterpret_cast<void**>(&pHostMemory)) == S_OK)
	{
		for (UINT subresourceIndex = 0; subresourceIndex < NumSubresources; ++subresourceIndex)
		{
			D3D12_MEMCPY_DEST MemCpyDest = {};
			MemCpyDest.pData		= pHostMemory + PlacedSubresourceLayouts[subresourceIndex].Offset;
			MemCpyDest.RowPitch		= static_cast<SIZE_T>(PlacedSubresourceLayouts[subresourceIndex].Footprint.RowPitch);
			MemCpyDest.SlicePitch	= static_cast<SIZE_T>(PlacedSubresourceLayouts[subresourceIndex].Footprint.RowPitch) * static_cast<SIZE_T>(NumRows[subresourceIndex]);

			for (UINT z = 0; z < PlacedSubresourceLayouts[subresourceIndex].Footprint.Depth; ++z)
			{
				BYTE* pDestSlice		= reinterpret_cast<BYTE*>(MemCpyDest.pData) + MemCpyDest.SlicePitch * static_cast<SIZE_T>(z);
				const BYTE* pSrcSlice	= reinterpret_cast<const BYTE*>(subresources[subresourceIndex].pData) + subresources[subresourceIndex].SlicePitch * LONG_PTR(z);
				for (UINT y = 0; y < NumRows[subresourceIndex]; ++y)
				{
					CopyMemory(pDestSlice + MemCpyDest.RowPitch * y, pSrcSlice + subresources[subresourceIndex].RowPitch * LONG_PTR(y), RowSizeInBytes[subresourceIndex]);
				}
			}
		}
		StagingResource->Unmap(0, nullptr);
	}

	StagingTexture StagingTexture			= {};
	StagingTexture.Path						= Path.generic_string();
	StagingTexture.Texture					= DeviceTexture(StagingResource);
	StagingTexture.NumSubresources			= NumSubresources;
	StagingTexture.PlacedSubresourceLayouts = std::move(PlacedSubresourceLayouts);
	StagingTexture.MipLevels				= MipLevels;
	StagingTexture.GenerateMips				= GenerateMips;
	m_Textures[StagingTexture.Path]			= TextureHandle;
	m_UnstagedTextures[TextureHandle]		= std::move(StagingTexture);
	SV_pRenderDevice->CreateShaderResourceView(TextureHandle); // Create SRV
	return TextureHandle;
}

void GpuTextureAllocator::LoadMaterial(Material& Material)
{
	auto GetDefaultTextureHandleIfNoFilePathIsProvided = [&](TextureTypes Type)
	{
		switch (Type)
		{
		case AlbedoIdx:		return m_SystemReservedTextures[DefaultAlbedo];
		case NormalIdx:		return m_SystemReservedTextures[DefaultNormal];
		case RoughnessIdx:	return m_SystemReservedTextures[DefaultWhite];
		case MetallicIdx:	return m_SystemReservedTextures[DefaultBlack];
		case EmissiveIdx:	return m_SystemReservedTextures[DefaultBlack];
		default:			assert(false && "Unknown Type"); return RenderResourceHandle();
		}
	};

	for (unsigned int i = 0; i < NumTextureTypes; ++i)
	{
		switch (Material.Textures[i].Flag)
		{
		case TextureFlags::Unknown:
		case TextureFlags::Disk:
		{
			bool SRGB = i == AlbedoIdx || i == EmissiveIdx;

			RenderResourceHandle TextureHandle = Material.Textures[i].Path.empty() ? 
				GetDefaultTextureHandleIfNoFilePathIsProvided(TextureTypes(i)) :
				LoadFromFile(Material.Textures[i].Path, SRGB, true);

			Material.TextureIndices[i] = SV_pRenderDevice->GetShaderResourceView(TextureHandle).HeapIndex;
		}
		break;

		case TextureFlags::Embedded:
		{
			assert(false && "Embedded Texture currently not supported"); // Currently not supported
		}
		break;
		}
	}
}

void GpuTextureAllocator::StageTexture(RenderResourceHandle TextureHandle, StagingTexture& StagingTexture, RenderContext& RenderContext)
{
	std::wstring Path = UTF8ToUTF16(StagingTexture.Path);
	PIXMarker(RenderContext->GetD3DCommandList(), Path.data());

	DeviceTexture* pTexture = SV_pRenderDevice->GetTexture(TextureHandle);
	DeviceTexture* pStagingResourceTexture = &StagingTexture.Texture;

	// Stage texture
	RenderContext->TransitionBarrier(pTexture, DeviceResource::State::CopyDest);
	for (size_t subresourceIndex = 0; subresourceIndex < StagingTexture.NumSubresources; ++subresourceIndex)
	{
		D3D12_TEXTURE_COPY_LOCATION Destination;
		Destination.pResource = pTexture->GetD3DResource();
		Destination.Type = D3D12_TEXTURE_COPY_TYPE_SUBRESOURCE_INDEX;
		Destination.SubresourceIndex = subresourceIndex;

		D3D12_TEXTURE_COPY_LOCATION Source;
		Source.pResource = pStagingResourceTexture->GetD3DResource();
		Source.Type = D3D12_TEXTURE_COPY_TYPE_PLACED_FOOTPRINT;
		Source.PlacedFootprint = StagingTexture.PlacedSubresourceLayouts[subresourceIndex];
		RenderContext->CopyTextureRegion(&Destination, 0, 0, 0, &Source, nullptr);
	}

	if (StagingTexture.GenerateMips)
	{
		if (IsUAVCompatable(pTexture->GetFormat()))
		{
			GenerateMipsUAV(TextureHandle, RenderContext);
		}
		else if (DirectX::IsSRGB(pTexture->GetFormat()))
		{
			GenerateMipsSRGB(TextureHandle, RenderContext);
		}
	}

	RenderContext->TransitionBarrier(pTexture, DeviceResource::State::PixelShaderResource | DeviceResource::State::NonPixelShaderResource);
	LOG_INFO("{} Loaded", StagingTexture.Path);
}

void GpuTextureAllocator::GenerateMipsUAV(RenderResourceHandle TextureHandle, RenderContext& RenderContext)
{
	// Credit: https://github.com/jpvanoosten/LearningDirectX12/blob/master/DX12Lib/src/CommandList.cpp
	DeviceTexture* pTexture = SV_pRenderDevice->GetTexture(TextureHandle);

	RenderContext.SetPipelineState(ComputePSOs::GenerateMips);

	GenerateMipsData GenerateMipsData	= {};
	GenerateMipsData.IsSRGB				= DirectX::IsSRGB(pTexture->GetFormat());

	for (uint32_t srcMip = 0; srcMip < pTexture->GetMipLevels() - 1u; )
	{
		uint64_t srcWidth	= pTexture->GetWidth() >> srcMip;
		uint32_t srcHeight	= pTexture->GetHeight() >> srcMip;
		uint32_t dstWidth	= static_cast<uint32_t>(srcWidth >> 1);
		uint32_t dstHeight	= srcHeight >> 1;

		// Determine the switch case to use in CS
		// 0b00(0): Both width and height are even.
		// 0b01(1): Width is odd, height is even.
		// 0b10(2): Width is even, height is odd.
		// 0b11(3): Both width and height are odd.
		GenerateMipsData.SrcDimension = (srcHeight & 1) << 1 | (srcWidth & 1);

		// How many mipmap levels to compute this pass (max 4 mips per pass)
		DWORD mipCount;

		// The number of times we can half the size of the texture and get
		// exactly a 50% reduction in size.
		// A 1 bit in the width or height indicates an odd dimension.
		// The case where either the width or the height is exactly 1 is handled
		// as a special case (as the dimension does not require reduction).
		_BitScanForward(&mipCount, (dstWidth == 1 ? dstHeight : dstWidth) | (dstHeight == 1 ? dstWidth : dstHeight));
		// Maximum number of mips to generate is 4.
		mipCount = std::min<DWORD>(4, mipCount + 1);
		// Clamp to total number of mips left over.
		mipCount = (srcMip + mipCount) >= pTexture->GetMipLevels() ? pTexture->GetMipLevels() - srcMip - 1 : mipCount;

		// Dimensions should not reduce to 0.
		// This can happen if the width and height are not the same.
		dstWidth = std::max<DWORD>(1, dstWidth);
		dstHeight = std::max<DWORD>(1, dstHeight);

		GenerateMipsData.SrcMipLevel	= srcMip;
		GenerateMipsData.NumMipLevels	= mipCount;
		GenerateMipsData.TexelSize.x	= 1.0f / (float)dstWidth;
		GenerateMipsData.TexelSize.y	= 1.0f / (float)dstHeight;

		for (uint32_t mip = 0; mip < mipCount; ++mip)
		{
			SV_pRenderDevice->CreateUnorderedAccessView(TextureHandle, {}, srcMip + mip + 1);
		}

		GenerateMipsData.InputIndex = SV_pRenderDevice->GetShaderResourceView(TextureHandle).HeapIndex;

		int32_t outputIndices[4] = { -1, -1, -1, -1 };
		for (uint32_t mip = 0; mip < mipCount; ++mip)
		{
			outputIndices[mip] = SV_pRenderDevice->GetUnorderedAccessView(TextureHandle, {}, srcMip + mip + 1).HeapIndex;
		}

		GenerateMipsData.Output1Index = outputIndices[0];
		GenerateMipsData.Output2Index = outputIndices[1];
		GenerateMipsData.Output3Index = outputIndices[2];
		GenerateMipsData.Output4Index = outputIndices[3];

		RenderContext->TransitionBarrier(pTexture, DeviceResource::State::NonPixelShaderResource, srcMip);
		for (uint32_t mip = 0; mip < mipCount; ++mip)
		{
			RenderContext->TransitionBarrier(pTexture, DeviceResource::State::UnorderedAccess, srcMip + mip + 1);
		}

		RenderContext->SetComputeRoot32BitConstants(0, sizeof(GenerateMipsData) / 4, &GenerateMipsData, 0);

		RenderContext->Dispatch2D(dstWidth, dstHeight, 8, 8);

		RenderContext->UAVBarrier(pTexture);
		RenderContext->FlushResourceBarriers();

		srcMip += mipCount;
	}
}

void GpuTextureAllocator::GenerateMipsSRGB(RenderResourceHandle TextureHandle, RenderContext& RenderContext)
{
	DeviceTexture* pTexture = SV_pRenderDevice->GetTexture(TextureHandle);

	RenderResourceHandle textureCopyHandle = SV_pRenderDevice->CreateDeviceTexture(pTexture->GetType(), [&](DeviceTextureProxy& proxy)
	{
		proxy.SetFormat(pTexture->GetFormat());
		proxy.SetWidth(pTexture->GetWidth());
		proxy.SetHeight(pTexture->GetHeight());
		proxy.SetDepthOrArraySize(pTexture->GetDepthOrArraySize());
		proxy.SetMipLevels(pTexture->GetMipLevels());
		proxy.BindFlags = DeviceResource::BindFlags::UnorderedAccess;
		proxy.InitialState = DeviceResource::State::CopyDest;
	});

	DeviceTexture* pDstTexture = SV_pRenderDevice->GetTexture(textureCopyHandle);

	RenderContext->CopyResource(pDstTexture, pTexture);

	GenerateMipsUAV(textureCopyHandle, RenderContext);

	RenderContext->CopyResource(pTexture, pDstTexture);

	m_TemporaryResources.push_back(textureCopyHandle);
}

void GpuTextureAllocator::EquirectangularToCubemap(RenderResourceHandle EquirectangularMap, RenderResourceHandle Cubemap, RenderContext& RenderContext)
{
	DeviceTexture* pCubemap = SV_pRenderDevice->GetTexture(Cubemap);
	if (IsUAVCompatable(pCubemap->GetFormat()))
	{
		EquirectangularToCubemapUAV(EquirectangularMap, Cubemap, RenderContext);
	}
	else if (DirectX::IsSRGB(pCubemap->GetFormat()))
	{
		EquirectangularToCubemapSRGB(EquirectangularMap, Cubemap, RenderContext);
	}
}

void GpuTextureAllocator::EquirectangularToCubemapUAV(RenderResourceHandle EquirectangularMap, RenderResourceHandle Cubemap, RenderContext& RenderContext)
{
	SV_pRenderDevice->CreateShaderResourceView(EquirectangularMap);

	DeviceTexture* pEquirectangularMap = SV_pRenderDevice->GetTexture(EquirectangularMap);
	DeviceTexture* pCubemap = SV_pRenderDevice->GetTexture(Cubemap);
	auto resourceDesc = pCubemap->GetD3DResource()->GetDesc();

	RenderContext.SetPipelineState(ComputePSOs::EquirectangularToCubemap);

	EquirectangularToCubemapData EquirectangularToCubemapData = {};

	for (uint32_t mipSlice = 0; mipSlice < resourceDesc.MipLevels; )
	{
		// Maximum number of mips to generate per pass is 5.
		uint32_t numMips = std::min<uint32_t>(5, resourceDesc.MipLevels - mipSlice);

		EquirectangularToCubemapData.FirstMip = mipSlice;
		EquirectangularToCubemapData.CubemapSize = std::max<uint32_t>(static_cast<uint32_t>(resourceDesc.Width), resourceDesc.Height) >> mipSlice;
		EquirectangularToCubemapData.NumMips = numMips;

		for (uint32_t mip = 0; mip < numMips; ++mip)
		{
			SV_pRenderDevice->CreateUnorderedAccessView(Cubemap, {}, mipSlice + mip);
		}

		int outputIndices[5] = { -1, -1, -1, -1, -1 };
		for (uint32_t mip = 0; mip < numMips; ++mip)
		{
			outputIndices[mip] = SV_pRenderDevice->GetUnorderedAccessView(Cubemap, {}, mipSlice + mip).HeapIndex;
		}

		EquirectangularToCubemapData.InputIndex = SV_pRenderDevice->GetShaderResourceView(EquirectangularMap).HeapIndex;

		EquirectangularToCubemapData.Output1Index = outputIndices[0];
		EquirectangularToCubemapData.Output2Index = outputIndices[1];
		EquirectangularToCubemapData.Output3Index = outputIndices[2];
		EquirectangularToCubemapData.Output4Index = outputIndices[3];
		EquirectangularToCubemapData.Output5Index = outputIndices[4];

		RenderContext->TransitionBarrier(pEquirectangularMap, DeviceResource::State::NonPixelShaderResource);
		for (uint32_t mip = 0; mip < numMips; ++mip)
		{
			RenderContext->TransitionBarrier(pCubemap, DeviceResource::State::UnorderedAccess);
		}

		RenderContext->SetComputeRoot32BitConstants(0, sizeof(EquirectangularToCubemapData) / 4, &EquirectangularToCubemapData, 0);

		UINT threadGroupCount = Math::RoundUpAndDivide(EquirectangularToCubemapData.CubemapSize, 16);
		RenderContext->Dispatch(threadGroupCount, threadGroupCount, 6);

		RenderContext->UAVBarrier(pCubemap);

		mipSlice += numMips;
	}
}

void GpuTextureAllocator::EquirectangularToCubemapSRGB(RenderResourceHandle EquirectangularMap, RenderResourceHandle Cubemap, RenderContext& RenderContext)
{
	DeviceTexture* pCubemap = SV_pRenderDevice->GetTexture(Cubemap);

	RenderResourceHandle textureCopyHandle = SV_pRenderDevice->CreateDeviceTexture(pCubemap->GetType(), [&](DeviceTextureProxy& proxy)
	{
		proxy.SetFormat(pCubemap->GetFormat());
		proxy.SetWidth(pCubemap->GetWidth());
		proxy.SetHeight(pCubemap->GetHeight());
		proxy.SetDepthOrArraySize(pCubemap->GetDepthOrArraySize());
		proxy.SetMipLevels(pCubemap->GetMipLevels());
		proxy.BindFlags = DeviceResource::BindFlags::UnorderedAccess;
		proxy.InitialState = DeviceResource::State::CopyDest;
	});

	DeviceTexture* pDstTexture = SV_pRenderDevice->GetTexture(textureCopyHandle);

	RenderContext->CopyResource(pDstTexture, pCubemap);

	EquirectangularToCubemapUAV(EquirectangularMap, textureCopyHandle, RenderContext);

	RenderContext->CopyResource(pCubemap, pDstTexture);

	m_TemporaryResources.push_back(textureCopyHandle);
}

bool GpuTextureAllocator::IsUAVCompatable(DXGI_FORMAT Format)
{
	switch (Format)
	{
	case DXGI_FORMAT_R32G32B32A32_FLOAT:
	case DXGI_FORMAT_R32G32B32A32_UINT:
	case DXGI_FORMAT_R32G32B32A32_SINT:
	case DXGI_FORMAT_R16G16B16A16_FLOAT:
	case DXGI_FORMAT_R16G16B16A16_UINT:
	case DXGI_FORMAT_R16G16B16A16_SINT:
	case DXGI_FORMAT_R8G8B8A8_UNORM:
	case DXGI_FORMAT_R8G8B8A8_UINT:
	case DXGI_FORMAT_R8G8B8A8_SINT:
	case DXGI_FORMAT_R32_FLOAT:
	case DXGI_FORMAT_R32_UINT:
	case DXGI_FORMAT_R32_SINT:
	case DXGI_FORMAT_R16_FLOAT:
	case DXGI_FORMAT_R16_UINT:
	case DXGI_FORMAT_R16_SINT:
	case DXGI_FORMAT_R8_UNORM:
	case DXGI_FORMAT_R8_UINT:
	case DXGI_FORMAT_R8_SINT:
		return true;
	}
	if (m_UAVSupportedFormat.find(Format) != m_UAVSupportedFormat.end())
		return true;
	return false;
}

DXGI_FORMAT GpuTextureAllocator::GetUAVCompatableFormat(DXGI_FORMAT Format)
{
	DXGI_FORMAT uavFormat = Format;

	switch (Format)
	{
	case DXGI_FORMAT_R8G8B8A8_TYPELESS:
	case DXGI_FORMAT_R8G8B8A8_UNORM_SRGB:
	case DXGI_FORMAT_B8G8R8A8_UNORM:
	case DXGI_FORMAT_B8G8R8X8_UNORM:
	case DXGI_FORMAT_B8G8R8A8_TYPELESS:
	case DXGI_FORMAT_B8G8R8A8_UNORM_SRGB:
	case DXGI_FORMAT_B8G8R8X8_TYPELESS:
	case DXGI_FORMAT_B8G8R8X8_UNORM_SRGB:
		uavFormat = DXGI_FORMAT_R8G8B8A8_UNORM;
		break;
	case DXGI_FORMAT_R32_TYPELESS:
	case DXGI_FORMAT_D32_FLOAT:
		uavFormat = DXGI_FORMAT_R32_FLOAT;
		break;
	}

	return uavFormat;
}