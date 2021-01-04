#include "pch.h"
#include "TextureManager.h"
#include "RendererRegistry.h"

TextureManager::TextureManager(RenderDevice* pRenderDevice)
	: pRenderDevice(pRenderDevice)
{
	LoadSystemTextures();
	LoadNoiseTextures();
}

void TextureManager::StageAssetTextures(RenderContext& RenderContext)
{
	for (auto& [handle, stagingTexture] : m_UnstagedTextures)
	{
		StageTexture(handle, stagingTexture, RenderContext);
	}
}

void TextureManager::Stage(Scene& Scene, RenderContext& RenderContext)
{
	if (!Status::SkyboxGenerated && !Scene.Skybox.Path.empty())
	{
		Status::SkyboxGenerated = true;

		m_SkyboxEquirectangular = LoadFromFile(Scene.Skybox.Path, false, true);
		m_Skybox = pRenderDevice->InitializeRenderResourceHandle(RenderResourceType::Texture, Scene.Skybox.Path.string());
		auto& stagingTexture = m_UnstagedTextures[m_SkyboxEquirectangular];
		StageTexture(m_SkyboxEquirectangular, stagingTexture, RenderContext);

		// Generate cubemap for equirectangular map
		auto pEquirectangularMap = pRenderDevice->GetTexture(m_SkyboxEquirectangular);
		pRenderDevice->CreateTexture(m_Skybox, Resource::Type::TextureCube, [&](TextureProxy& proxy)
		{
			proxy.SetFormat(pEquirectangularMap->GetFormat());
			proxy.SetWidth(1024);
			proxy.SetHeight(1024);
			proxy.SetMipLevels(0);
			proxy.BindFlags = Resource::Flags::UnorderedAccess;
			proxy.InitialState = Resource::State::Common;
		});

		EquirectangularToCubemap(Scene.Skybox.Path.string(), m_SkyboxEquirectangular, m_Skybox, RenderContext);
		pRenderDevice->CreateShaderResourceView(m_Skybox);
	}

	for (auto& material : Scene.Materials)
	{
		LoadMaterial(material);
	}

	// Gpu copy
	for (auto& [handle, stagingTexture] : m_UnstagedTextures)
	{
		StageTexture(handle, stagingTexture, RenderContext);
	}
	RenderContext->FlushResourceBarriers();
}

void TextureManager::DisposeResources()
{
	m_UnstagedTextures.clear();

	for (auto& handle : m_TemporaryResources)
	{
		pRenderDevice->Destroy(handle);
	}
}

TextureManager::StagingTexture TextureManager::CreateStagingTexture(std::string Name, D3D12_RESOURCE_DESC Desc, const DirectX::ScratchImage& ScratchImage, std::size_t MipLevels, bool GenerateMips)
{
	std::vector<D3D12_SUBRESOURCE_DATA> subresources(ScratchImage.GetImageCount());
	const DirectX::Image* pImages = ScratchImage.GetImages();
	for (size_t i = 0; i < ScratchImage.GetImageCount(); ++i)
	{
		subresources[i].RowPitch = pImages[i].rowPitch;
		subresources[i].SlicePitch = pImages[i].slicePitch;
		subresources[i].pData = pImages[i].pixels;
	}

	Microsoft::WRL::ComPtr<ID3D12Resource>			StagingResource;
	const UINT										NumSubresources = static_cast<UINT>(subresources.size());
	std::vector<D3D12_PLACED_SUBRESOURCE_FOOTPRINT> PlacedSubresourceLayouts(NumSubresources);
	std::vector<UINT>								NumRows(NumSubresources);
	std::vector<UINT64>								RowSizeInBytes(NumSubresources);
	UINT64											TotalBytes = 0;

	auto pD3DDevice = pRenderDevice->Device.GetApiHandle();
	pD3DDevice->GetCopyableFootprints(&Desc, 0, NumSubresources, 0,
		PlacedSubresourceLayouts.data(), NumRows.data(), RowSizeInBytes.data(), &TotalBytes);

	auto HeapProperties = CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_UPLOAD);
	Desc = CD3DX12_RESOURCE_DESC::Buffer(TotalBytes);
	pD3DDevice->CreateCommittedResource(&HeapProperties, D3D12_HEAP_FLAG_NONE,
		&Desc, D3D12_RESOURCE_STATE_GENERIC_READ, nullptr, IID_PPV_ARGS(StagingResource.ReleaseAndGetAddressOf()));

	// CPU Upload
	BYTE* pMemory = nullptr;
	if (StagingResource->Map(0, nullptr, reinterpret_cast<void**>(&pMemory)) == S_OK)
	{
		for (UINT subresourceIndex = 0; subresourceIndex < NumSubresources; ++subresourceIndex)
		{
			D3D12_MEMCPY_DEST MemCpyDest = {};
			MemCpyDest.pData = pMemory + PlacedSubresourceLayouts[subresourceIndex].Offset;
			MemCpyDest.RowPitch = static_cast<SIZE_T>(PlacedSubresourceLayouts[subresourceIndex].Footprint.RowPitch);
			MemCpyDest.SlicePitch = static_cast<SIZE_T>(PlacedSubresourceLayouts[subresourceIndex].Footprint.RowPitch) * static_cast<SIZE_T>(NumRows[subresourceIndex]);

			for (UINT z = 0; z < PlacedSubresourceLayouts[subresourceIndex].Footprint.Depth; ++z)
			{
				BYTE* pDestSlice = reinterpret_cast<BYTE*>(MemCpyDest.pData) + MemCpyDest.SlicePitch * static_cast<SIZE_T>(z);
				const BYTE* pSrcSlice = reinterpret_cast<const BYTE*>(subresources[subresourceIndex].pData) + subresources[subresourceIndex].SlicePitch * LONG_PTR(z);
				for (UINT y = 0; y < NumRows[subresourceIndex]; ++y)
				{
					CopyMemory(pDestSlice + MemCpyDest.RowPitch * y, pSrcSlice + subresources[subresourceIndex].RowPitch * LONG_PTR(y), RowSizeInBytes[subresourceIndex]);
				}
			}
		}
		StagingResource->Unmap(0, nullptr);
	}

	StagingTexture StagingTexture = {};
	StagingTexture.Name = Name;
	StagingTexture.Texture = Texture(StagingResource);
	StagingTexture.NumSubresources = NumSubresources;
	StagingTexture.PlacedSubresourceLayouts = std::move(PlacedSubresourceLayouts);
	StagingTexture.MipLevels = MipLevels;
	StagingTexture.GenerateMips = GenerateMips;

	return StagingTexture;
}

void TextureManager::LoadSystemTextures()
{
	const std::filesystem::path AssetPaths[AssetTextures::NumSystemTextures] =
	{
		Application::ExecutableFolderPath / "Assets/Textures/DefaultWhite.dds",
		Application::ExecutableFolderPath / "Assets/Textures/DefaultBlack.dds",
		Application::ExecutableFolderPath / "Assets/Textures/DefaultAlbedoMap.dds",
		Application::ExecutableFolderPath / "Assets/Textures/DefaultNormalMap.dds",
		Application::ExecutableFolderPath / "Assets/Textures/DefaultRoughnessMap.dds",
		Application::ExecutableFolderPath / "Assets/LUT/LTC_LUT_DisneyDiffuse_InverseMatrix.dds",
		Application::ExecutableFolderPath / "Assets/LUT/LTC_LUT_DisneyDiffuse_Terms.dds",
		Application::ExecutableFolderPath / "Assets/LUT/LTC_LUT_GGX_InverseMatrix.dds",
		Application::ExecutableFolderPath / "Assets/LUT/LTC_LUT_GGX_Terms.dds"
	};

	D3D12_RESOURCE_DESC ResourceDescs[AssetTextures::NumSystemTextures] = {};
	D3D12_RESOURCE_ALLOCATION_INFO1 ResourceAllocationInfo1[AssetTextures::NumSystemTextures] = {};
	DirectX::TexMetadata TexMetadatas[AssetTextures::NumSystemTextures] = {};
	DirectX::ScratchImage ScratchImages[AssetTextures::NumSystemTextures] = {};

	for (size_t i = 0; i < AssetTextures::NumSystemTextures; ++i)
	{
		const auto& Path = AssetPaths[i];
		
		DirectX::TexMetadata TexMetadata;
		DirectX::ScratchImage ScratchImage;
		ThrowIfFailed(DirectX::LoadFromDDSFile(AssetPaths[i].c_str(), DirectX::DDS_FLAGS::DDS_FLAGS_FORCE_RGB, &TexMetadata, ScratchImage));

		m_SystemTextures[i] = pRenderDevice->InitializeRenderResourceHandle(RenderResourceType::Texture, Path.string());

		ResourceDescs[i] = CD3DX12_RESOURCE_DESC::Tex2D(TexMetadata.format,
			TexMetadata.width,
			TexMetadata.height,
			TexMetadata.arraySize,
			TexMetadata.mipLevels);

		// Since we are using small resources we can take advantage of 4KB
		// resource alignments. As long as the most detailed mip can fit in an
		// allocation less than 64KB, 4KB alignments can be used.
		//
		// When dealing with MSAA textures the rules are similar, but the minimum
		// alignment is 64KB for a texture whose most detailed mip can fit in an
		// allocation less than 4MB.
		ResourceDescs[i].Alignment = D3D12_SMALL_RESOURCE_PLACEMENT_ALIGNMENT;
		TexMetadatas[i] = TexMetadata;
		ScratchImages[i] = std::move(ScratchImage);
	}

	auto ResourceAllocationInfo = pRenderDevice->Device.GetApiHandle()->GetResourceAllocationInfo1(0, ARRAYSIZE(ResourceDescs), ResourceDescs, ResourceAllocationInfo1);
	if (ResourceAllocationInfo.Alignment != D3D12_SMALL_RESOURCE_PLACEMENT_ALIGNMENT)
	{
		// If the alignment requested is not granted, then let D3D tell us
		// the alignment that needs to be used for these resources.
		for (auto& ResourceDesc : ResourceDescs)
		{
			ResourceDesc.Alignment = 0;
		}
		ResourceAllocationInfo = pRenderDevice->Device.GetApiHandle()->GetResourceAllocationInfo1(0, ARRAYSIZE(ResourceDescs), ResourceDescs, ResourceAllocationInfo1);
	}
	
	auto HeapDesc = CD3DX12_HEAP_DESC(ResourceAllocationInfo.SizeInBytes, D3D12_HEAP_TYPE_DEFAULT, 0, D3D12_HEAP_FLAG_DENY_BUFFERS | D3D12_HEAP_FLAG_DENY_RT_DS_TEXTURES);
	m_SystemTextureHeap = pRenderDevice->InitializeRenderResourceHandle(RenderResourceType::Heap, "System Texture Heap");
	pRenderDevice->CreateHeap(m_SystemTextureHeap, HeapDesc);

	for (size_t i = 0; i < AssetTextures::NumSystemTextures; ++i)
	{
		const auto& Path = AssetPaths[i];

		UINT64 HeapOffset = ResourceAllocationInfo1[i].Offset;
		pRenderDevice->CreateTexture(m_SystemTextures[i], Resource::Type::Texture2D, m_SystemTextureHeap, HeapOffset, [&](TextureProxy& proxy)
		{
			proxy.SetFormat(ResourceDescs[i].Format);
			proxy.SetWidth(ResourceDescs[i].Width);
			proxy.SetHeight(ResourceDescs[i].Height);
			proxy.SetMipLevels(ResourceDescs[i].MipLevels);
			proxy.InitialState = Resource::State::CopyDest;
			proxy.Alignment = ResourceDescs[i].Alignment;
		});

		StagingTexture StagingTexture = CreateStagingTexture(Path.string(), ResourceDescs[i], ScratchImages[i], TexMetadatas[i].mipLevels, false);

		m_TextureCache[StagingTexture.Name] = m_SystemTextures[i];
		m_UnstagedTextures[m_SystemTextures[i]] = std::move(StagingTexture);
		pRenderDevice->CreateShaderResourceView(m_SystemTextures[i]); // Create SRV
	}
}

void TextureManager::LoadNoiseTextures()
{
	const std::filesystem::path AssetPaths[AssetTextures::NumNoiseTextures] =
	{
		Application::ExecutableFolderPath / "Assets/LUT/Blue_Noise_RGBA_0.dds"
	};

	D3D12_RESOURCE_DESC ResourceDescs[AssetTextures::NumNoiseTextures] = {};
	D3D12_RESOURCE_ALLOCATION_INFO1 ResourceAllocationInfo1[AssetTextures::NumNoiseTextures] = {};
	DirectX::TexMetadata TexMetadatas[AssetTextures::NumNoiseTextures] = {};
	DirectX::ScratchImage ScratchImages[AssetTextures::NumNoiseTextures] = {};

	for (size_t i = 0; i < AssetTextures::NumNoiseTextures; ++i)
	{
		const auto& Path = AssetPaths[i];

		DirectX::TexMetadata TexMetadata;
		DirectX::ScratchImage ScratchImage;
		ThrowIfFailed(DirectX::LoadFromDDSFile(AssetPaths[i].c_str(), DirectX::DDS_FLAGS::DDS_FLAGS_FORCE_RGB, &TexMetadata, ScratchImage));

		m_NoiseTextures[i] = pRenderDevice->InitializeRenderResourceHandle(RenderResourceType::Texture, Path.string());

		ResourceDescs[i] = CD3DX12_RESOURCE_DESC::Tex2D(TexMetadata.format,
			TexMetadata.width,
			TexMetadata.height,
			TexMetadata.arraySize,
			TexMetadata.mipLevels);

		TexMetadatas[i] = TexMetadata;
		ScratchImages[i] = std::move(ScratchImage);
	}

	auto ResourceAllocationInfo = pRenderDevice->Device.GetApiHandle()->GetResourceAllocationInfo1(0, ARRAYSIZE(ResourceDescs), ResourceDescs, ResourceAllocationInfo1);

	auto HeapDesc = CD3DX12_HEAP_DESC(ResourceAllocationInfo.SizeInBytes, D3D12_HEAP_TYPE_DEFAULT, 0, D3D12_HEAP_FLAG_DENY_BUFFERS | D3D12_HEAP_FLAG_DENY_RT_DS_TEXTURES);
	m_NoiseTextureHeap = pRenderDevice->InitializeRenderResourceHandle(RenderResourceType::Heap, "System Texture Heap");
	pRenderDevice->CreateHeap(m_NoiseTextureHeap, HeapDesc);

	for (size_t i = 0; i < AssetTextures::NumNoiseTextures; ++i)
	{
		const auto& Path = AssetPaths[i];

		UINT64 HeapOffset = ResourceAllocationInfo1[i].Offset;
		pRenderDevice->CreateTexture(m_NoiseTextures[i], Resource::Type::Texture2D, m_NoiseTextureHeap, HeapOffset, [&](TextureProxy& proxy)
		{
			proxy.SetFormat(ResourceDescs[i].Format);
			proxy.SetWidth(ResourceDescs[i].Width);
			proxy.SetHeight(ResourceDescs[i].Height);
			proxy.SetMipLevels(ResourceDescs[i].MipLevels);
			proxy.InitialState = Resource::State::CopyDest;
			proxy.Alignment = ResourceDescs[i].Alignment;
		});

		StagingTexture StagingTexture = CreateStagingTexture(Path.string(), ResourceDescs[i], ScratchImages[i], TexMetadatas[i].mipLevels, false);

		m_TextureCache[StagingTexture.Name] = m_NoiseTextures[i];
		m_UnstagedTextures[m_NoiseTextures[i]] = std::move(StagingTexture);
		pRenderDevice->CreateShaderResourceView(m_NoiseTextures[i]); // Create SRV
	}
}

RenderResourceHandle TextureManager::LoadFromFile(const std::filesystem::path& Path, bool ForceSRGB, bool GenerateMips)
{
	assert(std::filesystem::exists(Path) && "File not found");

	std::string Name = Path.string();
	std::string Extension = Path.extension().string();

	if (auto iter = m_TextureCache.find(Name);
		iter != m_TextureCache.end())
	{
		return iter->second;
	}

	DirectX::ScratchImage	ScratchImage;
	DirectX::TexMetadata	TexMetadata;
	if (Extension == ".dds")
	{
		ThrowIfFailed(DirectX::LoadFromDDSFile(Path.c_str(), DirectX::DDS_FLAGS::DDS_FLAGS_FORCE_RGB, &TexMetadata, ScratchImage));
	}
	else if (Extension == ".tga")
	{
		ThrowIfFailed(DirectX::LoadFromTGAFile(Path.c_str(), &TexMetadata, ScratchImage));
	}
	else if (Extension == ".hdr")
	{
		ThrowIfFailed(DirectX::LoadFromHDRFile(Path.c_str(), &TexMetadata, ScratchImage));
	}
	else
	{
		ThrowIfFailed(DirectX::LoadFromWICFile(Path.c_str(), DirectX::WIC_FLAGS::WIC_FLAGS_FORCE_RGB, &TexMetadata, ScratchImage));
	}
	// if metadata mip levels is > 1 then is already generated, other wise the value is based on argument
	GenerateMips = (TexMetadata.mipLevels > 1) ? false : GenerateMips;
	size_t MipLevels = GenerateMips ? static_cast<size_t>(std::floor(std::log2(std::max(TexMetadata.width, TexMetadata.height)))) + 1 : TexMetadata.mipLevels;

	if (ForceSRGB)
	{
		TexMetadata.format = DirectX::MakeSRGB(TexMetadata.format);
	}

	// Create actual texture
	RenderResourceHandle		TextureHandle	= pRenderDevice->InitializeRenderResourceHandle(RenderResourceType::Texture, Name);
	Resource::Flags				BindFlags		= GenerateMips ? Resource::Flags::UnorderedAccess : Resource::Flags::None;

	switch (TexMetadata.dimension)
	{
	case DirectX::TEX_DIMENSION::TEX_DIMENSION_TEXTURE1D:
	{
		pRenderDevice->CreateTexture(TextureHandle, Resource::Type::Texture1D, [&](TextureProxy& proxy)
		{
			proxy.SetFormat(TexMetadata.format);
			proxy.SetWidth(static_cast<UINT64>(TexMetadata.width));
			proxy.SetDepthOrArraySize(static_cast<UINT16>(TexMetadata.arraySize));
			proxy.SetMipLevels(MipLevels);
			proxy.BindFlags = BindFlags;
			proxy.InitialState = Resource::State::CopyDest;
		});
	}
	break;

	case DirectX::TEX_DIMENSION::TEX_DIMENSION_TEXTURE2D:
	{
		pRenderDevice->CreateTexture(TextureHandle, Resource::Type::Texture2D, [&](TextureProxy& proxy)
		{
			proxy.SetFormat(TexMetadata.format);
			proxy.SetWidth(static_cast<UINT64>(TexMetadata.width));
			proxy.SetHeight(static_cast<UINT>(TexMetadata.height));
			proxy.SetDepthOrArraySize(static_cast<UINT16>(TexMetadata.arraySize));
			proxy.SetMipLevels(MipLevels);
			proxy.BindFlags = BindFlags;
			proxy.InitialState = Resource::State::CopyDest;
		});
	}
	break;

	case DirectX::TEX_DIMENSION::TEX_DIMENSION_TEXTURE3D:
	{
		pRenderDevice->CreateTexture(TextureHandle, Resource::Type::Texture3D, [&](TextureProxy& proxy)
		{
			proxy.SetFormat(TexMetadata.format);
			proxy.SetWidth(static_cast<UINT64>(TexMetadata.width));
			proxy.SetHeight(static_cast<UINT>(TexMetadata.height));
			proxy.SetDepthOrArraySize(static_cast<UINT16>(TexMetadata.depth));
			proxy.SetMipLevels(MipLevels);
			proxy.BindFlags = BindFlags;
			proxy.InitialState = Resource::State::CopyDest;
		});
	}
	break;

	default: assert(false && "Unknown DirectX::TEX_DIMENSION");
	}

	Texture* pTexture = pRenderDevice->GetTexture(TextureHandle);
#ifdef _DEBUG
	pTexture->GetApiHandle()->SetName(Path.c_str());
#endif

	auto Desc		= pTexture->GetApiHandle()->GetDesc();

	StagingTexture StagingTexture			= CreateStagingTexture(Name, Desc, ScratchImage, MipLevels, GenerateMips);
	m_TextureCache[StagingTexture.Name]		= TextureHandle;
	m_UnstagedTextures[TextureHandle]		= std::move(StagingTexture);
	pRenderDevice->CreateShaderResourceView(TextureHandle); // Create SRV
	return TextureHandle;
}

void TextureManager::LoadMaterial(Material& Material)
{
	auto GetDefaultTextureHandleIfNoFilePathIsProvided = [this](TextureTypes Type)
	{
		switch (Type)
		{
		case AlbedoIdx:		return GetDefaultAlbedoTexture();
		case NormalIdx:		return GetDefaultNormalTexture();
		case RoughnessIdx:	return GetDefaultRoughnessTexture();
		case MetallicIdx:	return GetDefaultBlackTexture();
		case EmissiveIdx:	return GetDefaultBlackTexture();
		default:			assert(false && "Unknown Type"); return RenderResourceHandle();
		}
	};

	for (unsigned int i = 0; i < NumTextureTypes; ++i)
	{
		bool SRGB = i == AlbedoIdx || i == EmissiveIdx;

		RenderResourceHandle TextureHandle = Material.Textures[i].Path.empty() ?
			GetDefaultTextureHandleIfNoFilePathIsProvided(TextureTypes(i)) :
			LoadFromFile(Material.Textures[i].Path, SRGB, true);

		Material.TextureIndices[i] = pRenderDevice->GetShaderResourceView(TextureHandle).HeapIndex;
	}
}

void TextureManager::StageTexture(RenderResourceHandle TextureHandle, StagingTexture& StagingTexture, RenderContext& RenderContext)
{
#ifdef _DEBUG
	std::wstring Path = UTF8ToUTF16(StagingTexture.Name);
	PIXScopedEvent(RenderContext->GetApiHandle(), 0, Path.data());
#endif

	Texture* pTexture = pRenderDevice->GetTexture(TextureHandle);
	Texture* pStagingResourceTexture = &StagingTexture.Texture;

	// Stage texture
	RenderContext->TransitionBarrier(pTexture, Resource::State::CopyDest);
	RenderContext->FlushResourceBarriers();
	for (size_t subresourceIndex = 0; subresourceIndex < StagingTexture.NumSubresources; ++subresourceIndex)
	{
		D3D12_TEXTURE_COPY_LOCATION Destination = {};
		Destination.pResource = pTexture->GetApiHandle();
		Destination.Type = D3D12_TEXTURE_COPY_TYPE_SUBRESOURCE_INDEX;
		Destination.SubresourceIndex = subresourceIndex;

		D3D12_TEXTURE_COPY_LOCATION Source = {};
		Source.pResource = pStagingResourceTexture->GetApiHandle();
		Source.Type = D3D12_TEXTURE_COPY_TYPE_PLACED_FOOTPRINT;
		Source.PlacedFootprint = StagingTexture.PlacedSubresourceLayouts[subresourceIndex];
		RenderContext->CopyTextureRegion(&Destination, 0, 0, 0, &Source, nullptr);
	}

	if (StagingTexture.GenerateMips)
	{
		if (pRenderDevice->Device.IsUAVCompatable(pTexture->GetFormat()))
		{
			GenerateMipsUAV(TextureHandle, RenderContext);
		}
		else if (DirectX::IsSRGB(pTexture->GetFormat()))
		{
			GenerateMipsSRGB(StagingTexture.Name, TextureHandle, RenderContext);
		}
	}

	RenderContext->TransitionBarrier(pTexture, Resource::State::PixelShaderResource | Resource::State::NonPixelShaderResource);
	LOG_INFO("{} Loaded", StagingTexture.Name);
}

void TextureManager::GenerateMipsUAV(RenderResourceHandle TextureHandle, RenderContext& RenderContext)
{
	// Credit: https://github.com/jpvanoosten/LearningDirectX12/blob/master/DX12Lib/src/CommandList.cpp
	Texture* pTexture = pRenderDevice->GetTexture(TextureHandle);

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
			pRenderDevice->CreateUnorderedAccessView(TextureHandle, {}, srcMip + mip + 1);
		}

		GenerateMipsData.InputIndex = pRenderDevice->GetShaderResourceView(TextureHandle).HeapIndex;

		int32_t outputIndices[4] = { -1, -1, -1, -1 };
		for (uint32_t mip = 0; mip < mipCount; ++mip)
		{
			outputIndices[mip] = pRenderDevice->GetUnorderedAccessView(TextureHandle, {}, srcMip + mip + 1).HeapIndex;
		}

		GenerateMipsData.Output1Index = outputIndices[0];
		GenerateMipsData.Output2Index = outputIndices[1];
		GenerateMipsData.Output3Index = outputIndices[2];
		GenerateMipsData.Output4Index = outputIndices[3];

		RenderContext->TransitionBarrier(pTexture, Resource::State::NonPixelShaderResource, srcMip);
		for (uint32_t mip = 0; mip < mipCount; ++mip)
		{
			RenderContext->TransitionBarrier(pTexture, Resource::State::UnorderedAccess, srcMip + mip + 1);
		}

		RenderContext->SetComputeRoot32BitConstants(0, sizeof(GenerateMipsData) / 4, &GenerateMipsData, 0);

		RenderContext->Dispatch2D(dstWidth, dstHeight, 8, 8);

		RenderContext->UAVBarrier(pTexture);
		RenderContext->FlushResourceBarriers();

		srcMip += mipCount;
	}
}

void TextureManager::GenerateMipsSRGB(const std::string& Name, RenderResourceHandle TextureHandle, RenderContext& RenderContext)
{
	Texture* pTexture = pRenderDevice->GetTexture(TextureHandle);

	RenderResourceHandle textureCopyHandle = pRenderDevice->InitializeRenderResourceHandle(RenderResourceType::Texture, Name + " Copy");
	pRenderDevice->CreateTexture(textureCopyHandle, pTexture->GetType(), [&](TextureProxy& proxy)
	{
		proxy.SetFormat(pTexture->GetFormat());
		proxy.SetWidth(pTexture->GetWidth());
		proxy.SetHeight(pTexture->GetHeight());
		proxy.SetDepthOrArraySize(pTexture->GetDepthOrArraySize());
		proxy.SetMipLevels(pTexture->GetMipLevels());
		proxy.BindFlags = Resource::Flags::UnorderedAccess;
		proxy.InitialState = Resource::State::CopyDest;
	});

	Texture* pDstTexture = pRenderDevice->GetTexture(textureCopyHandle);

	RenderContext->CopyResource(pDstTexture, pTexture);

	GenerateMipsUAV(textureCopyHandle, RenderContext);

	RenderContext->CopyResource(pTexture, pDstTexture);

	m_TemporaryResources.push_back(textureCopyHandle);
}

void TextureManager::EquirectangularToCubemap(const std::string& Name, RenderResourceHandle EquirectangularMap, RenderResourceHandle Cubemap, RenderContext& RenderContext)
{
	Texture* pCubemap = pRenderDevice->GetTexture(Cubemap);
	if (pRenderDevice->Device.IsUAVCompatable(pCubemap->GetFormat()))
	{
		EquirectangularToCubemapUAV(EquirectangularMap, Cubemap, RenderContext);
	}
	else if (DirectX::IsSRGB(pCubemap->GetFormat()))
	{
		EquirectangularToCubemapSRGB(Name, EquirectangularMap, Cubemap, RenderContext);
	}
}

void TextureManager::EquirectangularToCubemapUAV(RenderResourceHandle EquirectangularMap, RenderResourceHandle Cubemap, RenderContext& RenderContext)
{
	pRenderDevice->CreateShaderResourceView(EquirectangularMap);

	Texture* pEquirectangularMap = pRenderDevice->GetTexture(EquirectangularMap);
	Texture* pCubemap = pRenderDevice->GetTexture(Cubemap);
	auto resourceDesc = pCubemap->GetApiHandle()->GetDesc();

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
			pRenderDevice->CreateUnorderedAccessView(Cubemap, {}, mipSlice + mip);
		}

		int outputIndices[5] = { -1, -1, -1, -1, -1 };
		for (uint32_t mip = 0; mip < numMips; ++mip)
		{
			outputIndices[mip] = pRenderDevice->GetUnorderedAccessView(Cubemap, {}, mipSlice + mip).HeapIndex;
		}

		EquirectangularToCubemapData.InputIndex = pRenderDevice->GetShaderResourceView(EquirectangularMap).HeapIndex;

		EquirectangularToCubemapData.Output1Index = outputIndices[0];
		EquirectangularToCubemapData.Output2Index = outputIndices[1];
		EquirectangularToCubemapData.Output3Index = outputIndices[2];
		EquirectangularToCubemapData.Output4Index = outputIndices[3];
		EquirectangularToCubemapData.Output5Index = outputIndices[4];

		RenderContext->TransitionBarrier(pEquirectangularMap, Resource::State::NonPixelShaderResource);
		for (uint32_t mip = 0; mip < numMips; ++mip)
		{
			RenderContext->TransitionBarrier(pCubemap, Resource::State::UnorderedAccess);
		}

		RenderContext->SetComputeRoot32BitConstants(0, sizeof(EquirectangularToCubemapData) / 4, &EquirectangularToCubemapData, 0);

		UINT threadGroupCount = Math::RoundUpAndDivide(EquirectangularToCubemapData.CubemapSize, 16);
		RenderContext->Dispatch(threadGroupCount, threadGroupCount, 6);

		RenderContext->UAVBarrier(pCubemap);

		mipSlice += numMips;
	}
}

void TextureManager::EquirectangularToCubemapSRGB(const std::string& Name, RenderResourceHandle EquirectangularMap, RenderResourceHandle Cubemap, RenderContext& RenderContext)
{
	Texture* pCubemap = pRenderDevice->GetTexture(Cubemap);

	RenderResourceHandle textureCopyHandle = pRenderDevice->InitializeRenderResourceHandle(RenderResourceType::Texture, Name + " Copy");
	pRenderDevice->CreateTexture(textureCopyHandle, pCubemap->GetType(), [&](TextureProxy& proxy)
	{
		proxy.SetFormat(pCubemap->GetFormat());
		proxy.SetWidth(pCubemap->GetWidth());
		proxy.SetHeight(pCubemap->GetHeight());
		proxy.SetDepthOrArraySize(pCubemap->GetDepthOrArraySize());
		proxy.SetMipLevels(pCubemap->GetMipLevels());
		proxy.BindFlags = Resource::Flags::UnorderedAccess;
		proxy.InitialState = Resource::State::CopyDest;
	});

	Texture* pDstTexture = pRenderDevice->GetTexture(textureCopyHandle);

	RenderContext->CopyResource(pDstTexture, pCubemap);

	EquirectangularToCubemapUAV(EquirectangularMap, textureCopyHandle, RenderContext);

	RenderContext->CopyResource(pCubemap, pDstTexture);

	m_TemporaryResources.push_back(textureCopyHandle);
}

DXGI_FORMAT TextureManager::GetUAVCompatableFormat(DXGI_FORMAT Format)
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