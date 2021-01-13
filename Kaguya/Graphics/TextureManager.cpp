#include "pch.h"
#include "TextureManager.h"

#include "RendererRegistry.h"

TextureManager::TextureManager(RenderDevice* pRenderDevice)
	: pRenderDevice(pRenderDevice)
{
	LoadSystemTextures();
	LoadNoiseTextures();
}

void TextureManager::Stage(Scene& Scene, CommandContext* pCommandContext)
{
	for (auto& material : Scene.Materials)
	{
		LoadMaterial(material);
	}

	// Gpu copy
	for (auto& [handle, stagingTexture] : m_UnstagedTextures)
	{
		StageTexture(handle, stagingTexture, pCommandContext);
	}
	pCommandContext->FlushResourceBarriers();
}

void TextureManager::DisposeResources()
{
	m_UnstagedTextures.clear();
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

void TextureManager::StageTexture(RenderResourceHandle TextureHandle, StagingTexture& StagingTexture, CommandContext* pCommandContext)
{
#ifdef _DEBUG
	std::wstring Path = UTF8ToUTF16(StagingTexture.Name);
	PIXScopedEvent(pCommandContext->GetApiHandle(), 0, Path.data());
#endif

	Texture* pTexture = pRenderDevice->GetTexture(TextureHandle);
	Texture* pStagingResourceTexture = &StagingTexture.Texture;

	// Stage texture
	pCommandContext->TransitionBarrier(pTexture, Resource::State::CopyDest);
	pCommandContext->FlushResourceBarriers();
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
		pCommandContext->CopyTextureRegion(&Destination, 0, 0, 0, &Source, nullptr);
	}

	pCommandContext->TransitionBarrier(pTexture, Resource::State::PixelShaderResource | Resource::State::NonPixelShaderResource);
	LOG_INFO("{} Loaded", StagingTexture.Name);
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