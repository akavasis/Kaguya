#include "pch.h"
#include "GpuTextureAllocator.h"
#include "RendererRegistry.h"

GpuTextureAllocator::GpuTextureAllocator(RenderDevice& RefRenderDevice)
	: m_RefRenderDevice(RefRenderDevice),
	m_TextureViewAllocator(&m_RefRenderDevice.GetDevice(), D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV)
{
	m_MipsNullUAV = m_TextureViewAllocator.Allocate(4);
	for (UINT i = 0; i < 4; ++i)
	{
		D3D12_UNORDERED_ACCESS_VIEW_DESC uavDesc = {};
		uavDesc.ViewDimension = D3D12_UAV_DIMENSION_TEXTURE2D;
		uavDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
		uavDesc.Texture2D.MipSlice = i;
		uavDesc.Texture2D.PlaneSlice = 0;

		m_RefRenderDevice.GetDevice().GetD3DDevice()->CreateUnorderedAccessView(nullptr, nullptr, &uavDesc, m_MipsNullUAV[i]);
	}

	m_EquirectangularToCubeMapNullUAV = m_TextureViewAllocator.Allocate(5);
	for (UINT i = 0; i < 5; ++i)
	{
		D3D12_UNORDERED_ACCESS_VIEW_DESC uavDesc = {};
		uavDesc.ViewDimension = D3D12_UAV_DIMENSION_TEXTURE2DARRAY;
		uavDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
		uavDesc.Texture2DArray.ArraySize = 6; // Cubemap.
		uavDesc.Texture2DArray.FirstArraySlice = 0;
		uavDesc.Texture2DArray.MipSlice = i;
		uavDesc.Texture2DArray.PlaneSlice = 0;

		m_RefRenderDevice.GetDevice().GetD3DDevice()->CreateUnorderedAccessView(nullptr, nullptr, &uavDesc, m_EquirectangularToCubeMapNullUAV[i]);
	}

	DXGI_FORMAT optionalFormats[] = {
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
	const int numOptionalFormats = ARRAYSIZE(optionalFormats);
	for (int i = 0; i < numOptionalFormats; ++i)
	{
		D3D12_FEATURE_DATA_FORMAT_SUPPORT featureDataFormatSupport;
		featureDataFormatSupport.Format = optionalFormats[i];

		ThrowCOMIfFailed(m_RefRenderDevice.GetDevice().GetD3DDevice()->CheckFeatureSupport(
			D3D12_FEATURE_FORMAT_SUPPORT,
			&featureDataFormatSupport,
			sizeof(D3D12_FEATURE_DATA_FORMAT_SUPPORT)));

		auto checkFormatSupport1 = [&](D3D12_FORMAT_SUPPORT1 FormatSupport)
		{
			return (featureDataFormatSupport.Support1 & FormatSupport) != 0;
		};
		auto checkFormatSupport2 = [&](D3D12_FORMAT_SUPPORT2 FormatSupport)
		{
			return (featureDataFormatSupport.Support2 & FormatSupport) != 0;
		};

		bool supportsUAV = checkFormatSupport1(D3D12_FORMAT_SUPPORT1_TYPED_UNORDERED_ACCESS_VIEW) &&
			checkFormatSupport2(D3D12_FORMAT_SUPPORT2_UAV_TYPED_LOAD) &&
			checkFormatSupport2(D3D12_FORMAT_SUPPORT2_UAV_TYPED_STORE);

		if (supportsUAV)
		{
			m_UAVSupportedFormat.insert(optionalFormats[i]);
		}
	}
}

void GpuTextureAllocator::Stage(Scene* pScene, RenderCommandContext* pRenderCommandContext)
{
	{
		// Need to generate mips for equirectangular because EquirectangularToCubemap shader assumes the SrvTexture contains mips
		RenderResourceHandle hdrTextureHandle = LoadFromFile(pScene->Skybox.Path, false, true);
		Texture* pHDRTexture = m_RefRenderDevice.GetTexture(hdrTextureHandle);

		Texture::Properties radianceTextureProp{};
		radianceTextureProp.Type = pHDRTexture->Type;
		radianceTextureProp.Format = pHDRTexture->Format;
		radianceTextureProp.Width = radianceTextureProp.Height = 1024;
		radianceTextureProp.DepthOrArraySize = 6;
		radianceTextureProp.MipLevels = 0;
		radianceTextureProp.Flags = D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS;
		radianceTextureProp.IsCubemap = true;

		radianceTextureProp.pOptimizedClearValue = nullptr;
		radianceTextureProp.InitialState = D3D12_RESOURCE_STATE_COMMON;
		RenderResourceHandle radianceTextureHandle = m_RefRenderDevice.CreateTexture(radianceTextureProp);

		EquirectangularToCubemap(hdrTextureHandle, radianceTextureHandle, pRenderCommandContext);
	}

	for (auto iter = pScene->Models.begin(); iter != pScene->Models.end(); ++iter)
	{
		auto& model = (*iter);

		for (auto& material : model.Materials)
		{
			LoadMaterial(material);
		}
	}

	for (auto iter = m_UnstagedTextures.begin(); iter != m_UnstagedTextures.end(); ++iter)
	{
		auto handle = iter->first;
		auto& stagingTexture = iter->second;

		Texture* pTexture = m_RefRenderDevice.GetTexture(handle);
		Texture* pStagingResourceTexture = &stagingTexture.texture;

		// Stage texture
		std::vector<D3D12_SUBRESOURCE_DATA> subresources(stagingTexture.scratchImage.GetImageCount());
		const DirectX::Image* pImages = stagingTexture.scratchImage.GetImages();
		for (size_t i = 0; i < stagingTexture.scratchImage.GetImageCount(); ++i)
		{
			subresources[i].RowPitch = pImages[i].rowPitch;
			subresources[i].SlicePitch = pImages[i].slicePitch;
			subresources[i].pData = pImages[i].pixels;
		}

		UpdateSubresources(pRenderCommandContext->GetD3DCommandList(),
			pTexture->GetD3DResource(), pStagingResourceTexture->GetD3DResource(), 0,
			0, subresources.size(), subresources.data());

		if (stagingTexture.generateMips)
		{
			GenerateMips(handle, pRenderCommandContext);
		}

		pRenderCommandContext->TransitionBarrier(pTexture, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);

		m_StagedTextures[stagingTexture.Path] = handle;
		CORE_INFO("{} Loaded", stagingTexture.Path);
	}
}

void GpuTextureAllocator::Bind(RenderCommandContext* pRenderCommandContext) const
{
	// TODO: Add bindless strategy
}

RenderResourceHandle GpuTextureAllocator::LoadFromFile(const std::filesystem::path& Path, bool ForceSRGB, bool GenerateMips)
{	
	if (!std::filesystem::exists(Path))
	{
		CORE_ERROR("File: {} Not found");
		assert(false);
	}

	StagingTexture stagingTexture;
	stagingTexture.Path = Path.generic_string();
	if (Path.extension() == ".dds")
	{
		ThrowCOMIfFailed(DirectX::LoadFromDDSFile(Path.c_str(), DirectX::DDS_FLAGS::DDS_FLAGS_FORCE_RGB, &stagingTexture.metadata, stagingTexture.scratchImage));
	}
	else if (Path.extension() == ".tga")
	{
		ThrowCOMIfFailed(DirectX::LoadFromTGAFile(Path.c_str(), &stagingTexture.metadata, stagingTexture.scratchImage));
	}
	else if (Path.extension() == ".hdr")
	{
		ThrowCOMIfFailed(DirectX::LoadFromHDRFile(Path.c_str(), &stagingTexture.metadata, stagingTexture.scratchImage));
	}
	else
	{
		ThrowCOMIfFailed(DirectX::LoadFromWICFile(Path.c_str(), DirectX::WIC_FLAGS::WIC_FLAGS_FORCE_RGB, &stagingTexture.metadata, stagingTexture.scratchImage));
	}
	// if metadata mip levels is > 1 then is already generated, other wise generate mip maps
	stagingTexture.generateMips = (stagingTexture.metadata.mipLevels > 1) ? false : GenerateMips;
	stagingTexture.mipLevels = stagingTexture.generateMips ? static_cast<size_t>(std::floor(std::log2(std::max(stagingTexture.metadata.width, stagingTexture.metadata.height)))) + 1 : 1;

	if (ForceSRGB)
	{
		stagingTexture.metadata.format = DirectX::MakeSRGB(stagingTexture.metadata.format);
		if (!DirectX::IsSRGB(stagingTexture.metadata.format))
		{
			CORE_WARN("Failed to convert to SRGB format");
		}
	}

	Texture::Properties textureProp{};
	textureProp.Format = stagingTexture.metadata.format;
	textureProp.MipLevels = stagingTexture.mipLevels;
	textureProp.Flags = D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS;
	textureProp.IsCubemap = stagingTexture.metadata.IsCubemap();
	textureProp.InitialState = D3D12_RESOURCE_STATE_COPY_DEST;

	switch (stagingTexture.metadata.dimension)
	{
	case DirectX::TEX_DIMENSION::TEX_DIMENSION_TEXTURE1D:
		textureProp.Type = Resource::Type::Texture1D;
		textureProp.Width = static_cast<UINT64>(stagingTexture.metadata.width);
		textureProp.DepthOrArraySize = static_cast<UINT16>(stagingTexture.metadata.arraySize);
		break;
	case DirectX::TEX_DIMENSION::TEX_DIMENSION_TEXTURE2D:
		textureProp.Type = Resource::Type::Texture2D;
		textureProp.Width = static_cast<UINT64>(stagingTexture.metadata.width);
		textureProp.Height = static_cast<UINT>(stagingTexture.metadata.height);
		textureProp.DepthOrArraySize = static_cast<UINT16>(stagingTexture.metadata.arraySize);
		break;
	case DirectX::TEX_DIMENSION::TEX_DIMENSION_TEXTURE3D:
		textureProp.Type = Resource::Type::Texture3D;
		textureProp.Width = static_cast<UINT64>(stagingTexture.metadata.width);
		textureProp.Height = static_cast<UINT>(stagingTexture.metadata.height);
		textureProp.DepthOrArraySize = static_cast<UINT16>(stagingTexture.metadata.depth);
		break;
	}

	RenderResourceHandle handle = m_RefRenderDevice.CreateTexture(textureProp);
	Texture* pTexture = m_RefRenderDevice.GetTexture(handle);

	UINT64 totalBytes = GetRequiredIntermediateSize(pTexture->GetD3DResource(), 0, pTexture->GetNumSubresources());

	Microsoft::WRL::ComPtr<ID3D12Resource> stagingResource;
	m_RefRenderDevice.GetDevice().GetD3DDevice()->CreateCommittedResource(&CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_UPLOAD), D3D12_HEAP_FLAG_NONE,
		&CD3DX12_RESOURCE_DESC::Buffer(totalBytes), D3D12_RESOURCE_STATE_GENERIC_READ, nullptr, IID_PPV_ARGS(stagingResource.ReleaseAndGetAddressOf()));

	stagingTexture.texture = Texture(stagingResource);

	m_UnstagedTextures[handle] = std::move(stagingTexture);
	return handle;
}

void GpuTextureAllocator::LoadMaterial(Material& Material)
{
	for (unsigned int i = 0; i < NumTextureTypes; ++i)
	{
		switch (Material.Textures[i].Flag)
		{
		case TextureFlags::Disk:
		{
			bool srgb = i == Albedo || i == Emissive;

			LoadFromFile(Material.Textures[i].Path, srgb, true);
		}
		break;

		case TextureFlags::Embedded:
		{
			assert(false); // Currently not supported
		}
		break;
		}
	}
}

void GpuTextureAllocator::GenerateMips(RenderResourceHandle TextureHandle, RenderCommandContext* pRenderCommandContext)
{
	Texture* pTexture = m_RefRenderDevice.GetTexture(TextureHandle);
	if (IsUAVCompatable(pTexture->Format))
	{
		GenerateMipsUAV(TextureHandle, pRenderCommandContext);
	}
	else if (IsSRGB(pTexture->Format))
	{
		GenerateMipsSRGB(TextureHandle, pRenderCommandContext);
	}
}

void GpuTextureAllocator::GenerateMipsUAV(RenderResourceHandle TextureHandle, RenderCommandContext* pRenderCommandContext)
{
	// Credit: https://github.com/jpvanoosten/LearningDirectX12/blob/master/DX12Lib/src/CommandList.cpp
	Descriptor descriptor = m_TextureViewAllocator.Allocate(1);
	m_RefRenderDevice.CreateSRVForTexture(TextureHandle, descriptor[0]);
	m_TemporaryDescriptors.push_back(descriptor);

	Texture* pTexture = m_RefRenderDevice.GetTexture(TextureHandle);
	auto resourceDesc = pTexture->GetD3DResource()->GetDesc();

	pRenderCommandContext->SetPipelineState(m_RefRenderDevice.GetComputePSO(ComputePSOs::GenerateMips));
	pRenderCommandContext->SetComputeRootSignature(m_RefRenderDevice.GetRootSignature(RootSignatures::GenerateMips));

	GenerateMipsCPUData generateMipsCB;
	generateMipsCB.IsSRGB = DirectX::IsSRGB(resourceDesc.Format);

	for (uint32_t srcMip = 0; srcMip < resourceDesc.MipLevels - 1u; )
	{
		uint64_t srcWidth = resourceDesc.Width >> srcMip;
		uint32_t srcHeight = resourceDesc.Height >> srcMip;
		uint32_t dstWidth = static_cast<uint32_t>(srcWidth >> 1);
		uint32_t dstHeight = srcHeight >> 1;

		// Determine the switch case to use in CS
		// 0b00(0): Both width and height are even.
		// 0b01(1): Width is odd, height is even.
		// 0b10(2): Width is even, height is odd.
		// 0b11(3): Both width and height are odd.
		generateMipsCB.SrcDimension = (srcHeight & 1) << 1 | (srcWidth & 1);

		// How many mipmap levels to compute this pass (max 4 mips per pass)
		DWORD mipCount;

		// The number of times we can half the size of the texture and get
		// exactly a 50% reduction in size.
		// A 1 bit in the width or height indicates an odd dimension.
		// The case where either the width or the height is exactly 1 is handled
		// as a special case (as the dimension does not require reduction).
		_BitScanForward(&mipCount, (dstWidth == 1 ? dstHeight : dstWidth) |
			(dstHeight == 1 ? dstWidth : dstHeight));
		// Maximum number of mips to generate is 4.
		mipCount = std::min<DWORD>(4, mipCount + 1);
		// Clamp to total number of mips left over.
		mipCount = (srcMip + mipCount) >= resourceDesc.MipLevels ?
			resourceDesc.MipLevels - srcMip - 1 : mipCount;

		// Dimensions should not reduce to 0.
		// This can happen if the width and height are not the same.
		dstWidth = std::max<DWORD>(1, dstWidth);
		dstHeight = std::max<DWORD>(1, dstHeight);

		generateMipsCB.SrcMipLevel = srcMip;
		generateMipsCB.NumMipLevels = mipCount;
		generateMipsCB.TexelSize.x = 1.0f / (float)dstWidth;
		generateMipsCB.TexelSize.y = 1.0f / (float)dstHeight;

		pRenderCommandContext->SetComputeRoot32BitConstants(RootParameters::GenerateMips::GenerateMipsRootParameter_GenerateMipsCB, sizeof(GenerateMipsCPUData) / 4, &generateMipsCB, 0);

		pRenderCommandContext->TransitionBarrier(pTexture, D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE, srcMip);
		pRenderCommandContext->SetSRV(RootParameters::GenerateMips::GenerateMipsRootParameter_SrcMip, 0, descriptor.NumDescriptors(), descriptor);

		for (uint32_t mip = 0; mip < mipCount; ++mip)
		{
			Descriptor uavDescriptor = m_TextureViewAllocator.Allocate(1);
			m_RefRenderDevice.CreateUAVForTexture(TextureHandle, uavDescriptor[0], srcMip + mip + 1);
			m_TemporaryDescriptors.push_back(uavDescriptor);

			pRenderCommandContext->TransitionBarrier(pTexture, D3D12_RESOURCE_STATE_UNORDERED_ACCESS, srcMip + mip + 1);
			pRenderCommandContext->SetUAV(RootParameters::GenerateMips::GenerateMipsRootParameter_OutMip, mip, uavDescriptor.NumDescriptors(), uavDescriptor);
		}

		// Pad any unused mip levels with a default UAV. Doing this keeps the DX12 runtime happy.
		if (mipCount < 4)
		{
			pRenderCommandContext->SetUAV(RootParameters::GenerateMips::GenerateMipsRootParameter_OutMip, mipCount, 4 - mipCount, m_MipsNullUAV);
		}

		UINT threadGroupCountX = Math::DivideByMultiple(dstWidth, 8);
		UINT threadGroupCountY = Math::DivideByMultiple(dstHeight, 8);
		pRenderCommandContext->Dispatch(threadGroupCountX, threadGroupCountY, 1);

		pRenderCommandContext->UAVBarrier(pTexture);

		srcMip += mipCount;
	}
}

void GpuTextureAllocator::GenerateMipsSRGB(RenderResourceHandle TextureHandle, RenderCommandContext* pRenderCommandContext)
{
	Texture* pTexture = m_RefRenderDevice.GetTexture(TextureHandle);

	Texture::Properties textureProp{};
	textureProp.Type = pTexture->Type;
	textureProp.Format = GetUAVCompatableFormat(pTexture->Format);
	textureProp.Width = static_cast<UINT64>(pTexture->Width);
	textureProp.Height = static_cast<UINT>(pTexture->Height);
	textureProp.DepthOrArraySize = static_cast<UINT16>(pTexture->DepthOrArraySize);
	textureProp.MipLevels = pTexture->MipLevels;
	textureProp.Flags = D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS;
	textureProp.IsCubemap = pTexture->IsCubemap;
	textureProp.InitialState = D3D12_RESOURCE_STATE_COPY_DEST;

	RenderResourceHandle textureCopyHandle = m_RefRenderDevice.CreateTexture(textureProp);

	Texture* pDstTexture = m_RefRenderDevice.GetTexture(textureCopyHandle);

	pRenderCommandContext->CopyResource(pDstTexture, pTexture);

	GenerateMipsUAV(textureCopyHandle, pRenderCommandContext);

	pRenderCommandContext->CopyResource(pTexture, pDstTexture);

	m_TemporaryResources.push_back(textureCopyHandle);
}

void GpuTextureAllocator::EquirectangularToCubemap(RenderResourceHandle EquirectangularMap, RenderResourceHandle Cubemap, RenderCommandContext* pRenderCommandContext)
{
	Texture* pCubemap = m_RefRenderDevice.GetTexture(Cubemap);
	if (IsUAVCompatable(pCubemap->Format))
	{
		EquirectangularToCubemapUAV(EquirectangularMap, Cubemap, pRenderCommandContext);
	}
	else if (IsSRGB(pCubemap->Format))
	{
		EquirectangularToCubemapSRGB(EquirectangularMap, Cubemap, pRenderCommandContext);
	}
}

void GpuTextureAllocator::EquirectangularToCubemapUAV(RenderResourceHandle EquirectangularMap, RenderResourceHandle Cubemap, RenderCommandContext* pRenderCommandContext)
{
	Descriptor descriptor = m_TextureViewAllocator.Allocate(1);
	m_RefRenderDevice.CreateSRVForTexture(EquirectangularMap, descriptor[0]);
	m_TemporaryDescriptors.push_back(descriptor);

	Texture* pEquirectangularMap = m_RefRenderDevice.GetTexture(EquirectangularMap);
	Texture* pCubemap = m_RefRenderDevice.GetTexture(Cubemap);
	auto resourceDesc = pCubemap->GetD3DResource()->GetDesc();

	pRenderCommandContext->SetPipelineState(m_RefRenderDevice.GetComputePSO(ComputePSOs::EquirectangularToCubemap));
	pRenderCommandContext->SetComputeRootSignature(m_RefRenderDevice.GetRootSignature(RootSignatures::EquirectangularToCubemap));

	EquirectangularToCubemapCPUData panoToCubemapCB;

	for (uint32_t mipSlice = 0; mipSlice < resourceDesc.MipLevels; )
	{
		// Maximum number of mips to generate per pass is 5.
		uint32_t numMips = std::min<uint32_t>(5, resourceDesc.MipLevels - mipSlice);

		panoToCubemapCB.FirstMip = mipSlice;
		panoToCubemapCB.CubemapSize = std::max<uint32_t>(static_cast<uint32_t>(resourceDesc.Width), resourceDesc.Height) >> mipSlice;
		panoToCubemapCB.NumMips = numMips;

		pRenderCommandContext->SetComputeRoot32BitConstants(RootParameters::EquirectangularToCubemap::EquirectangularToCubemapRootParameter_PanoToCubemapCB, 3, &panoToCubemapCB, 0);

		pRenderCommandContext->TransitionBarrier(pEquirectangularMap, D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE);
		pRenderCommandContext->SetSRV(RootParameters::EquirectangularToCubemap::EquirectangularToCubemapRootParameter_SrcTexture, 0, descriptor.NumDescriptors(), descriptor);

		for (uint32_t mip = 0; mip < numMips; ++mip)
		{
			Descriptor uavDescriptor = m_TextureViewAllocator.Allocate(1);
			m_RefRenderDevice.CreateUAVForTexture(Cubemap, uavDescriptor[0], mipSlice + mip);
			m_TemporaryDescriptors.push_back(uavDescriptor);

			pRenderCommandContext->TransitionBarrier(pCubemap, D3D12_RESOURCE_STATE_UNORDERED_ACCESS);
			pRenderCommandContext->SetUAV(RootParameters::EquirectangularToCubemap::EquirectangularToCubemapRootParameter_DstMips, mip, uavDescriptor.NumDescriptors(), uavDescriptor);
		}

		if (numMips < 5)
		{
			// Pad unused mips. This keeps DX12 runtime happy.
			pRenderCommandContext->SetUAV(RootParameters::EquirectangularToCubemap::EquirectangularToCubemapRootParameter_DstMips, panoToCubemapCB.NumMips, 5 - numMips, m_EquirectangularToCubeMapNullUAV);
		}
		
		UINT threadGroupCount = Math::DivideByMultiple(panoToCubemapCB.CubemapSize, 16);
		pRenderCommandContext->Dispatch(threadGroupCount, threadGroupCount, 6);

		mipSlice += numMips;
	}
}

void GpuTextureAllocator::EquirectangularToCubemapSRGB(RenderResourceHandle EquirectangularMap, RenderResourceHandle Cubemap, RenderCommandContext* pRenderCommandContext)
{
	Texture* pCubemap = m_RefRenderDevice.GetTexture(Cubemap);

	Texture::Properties textureProp{};
	textureProp.Type = pCubemap->Type;
	textureProp.Format = GetUAVCompatableFormat(pCubemap->Format);
	textureProp.Width = static_cast<UINT64>(pCubemap->Width);
	textureProp.Height = static_cast<UINT>(pCubemap->Height);
	textureProp.DepthOrArraySize = static_cast<UINT16>(pCubemap->DepthOrArraySize);
	textureProp.MipLevels = pCubemap->MipLevels;
	textureProp.Flags = D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS;
	textureProp.IsCubemap = pCubemap->IsCubemap;
	textureProp.InitialState = D3D12_RESOURCE_STATE_COPY_DEST;

	RenderResourceHandle textureCopyHandle = m_RefRenderDevice.CreateTexture(textureProp);

	Texture* pDstTexture = m_RefRenderDevice.GetTexture(textureCopyHandle);

	pRenderCommandContext->CopyResource(pDstTexture, pCubemap);

	EquirectangularToCubemapUAV(EquirectangularMap, textureCopyHandle, pRenderCommandContext);

	pRenderCommandContext->CopyResource(pCubemap, pDstTexture);

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

bool GpuTextureAllocator::IsSRGB(DXGI_FORMAT Format)
{
	switch (Format)
	{
	case DXGI_FORMAT_R8G8B8A8_UNORM_SRGB:
	case DXGI_FORMAT_B8G8R8A8_UNORM_SRGB:
	case DXGI_FORMAT_B8G8R8X8_UNORM_SRGB:
		return true;
	}
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