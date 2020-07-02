#include "pch.h"
#include "TexturePool.h"
#include "RendererRegistry.h"

TexturePool::TexturePool(std::filesystem::path ExecutableFolderPath, RenderDevice& RefRenderDevice)
	: m_ExecutableFolderPath(ExecutableFolderPath),
	m_RefRenderDevice(RefRenderDevice),
	m_TextureViewAllocator(&m_RefRenderDevice.GetDevice(), D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV)
{
	m_MipMapDefaultUAV = m_TextureViewAllocator.Allocate(4);
	for (UINT i = 0; i < 4; ++i)
	{
		D3D12_UNORDERED_ACCESS_VIEW_DESC uavDesc = {};
		uavDesc.ViewDimension = D3D12_UAV_DIMENSION_TEXTURE2D;
		uavDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
		uavDesc.Texture2D.MipSlice = i;
		uavDesc.Texture2D.PlaneSlice = 0;

		m_RefRenderDevice.GetDevice().GetD3DDevice()->CreateUnorderedAccessView(nullptr, nullptr, &uavDesc, m_MipMapDefaultUAV[i]);
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

void TexturePool::Stage(RenderCommandContext* pRenderCommandContext)
{
	for (auto iter = m_UnstagedTextures.begin(); iter != m_UnstagedTextures.end(); ++iter)
	{
		StagingTexture& statgingTexture = iter->second;

		std::vector<D3D12_SUBRESOURCE_DATA> subresources(statgingTexture.scratchImage.GetImageCount());
		const DirectX::Image* pImages = statgingTexture.scratchImage.GetImages();
		for (size_t i = 0; i < statgingTexture.scratchImage.GetImageCount(); ++i)
		{
			subresources[i].RowPitch = pImages[i].rowPitch;
			subresources[i].SlicePitch = pImages[i].slicePitch;
			subresources[i].pData = pImages[i].pixels;
		}

		Texture* pDstTexture = m_RefRenderDevice.GetTexture(iter->first);
		Texture* pIntermediate = &statgingTexture.texture;

		UpdateSubresources(pRenderCommandContext->GetD3DCommandList(),
			pDstTexture->GetD3DResource(), pIntermediate->GetD3DResource(), 0,
			0, subresources.size(), subresources.data());

		if (statgingTexture.generateMips)
		{
			GenerateMips(iter->first, pRenderCommandContext);
		}
		m_StagedTextures.push_back(iter->first);

		pRenderCommandContext->TransitionBarrier(pDstTexture, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);

		CORE_INFO("{} Loaded", statgingTexture.fileName);
	}
}

RenderResourceHandle TexturePool::LoadFromFile(const char* pPath, bool ForceSRGB, bool GenerateMips)
{
	std::filesystem::path filePath = m_ExecutableFolderPath / pPath;
	if (!std::filesystem::exists(filePath))
	{
		CORE_ERROR("File: {} Not found");
		return RenderResourceHandle();
	}

	DirectX::TexMetadata metadata;
	DirectX::ScratchImage scratchImage;
	if (filePath.extension() == ".dds")
	{
		ThrowCOMIfFailed(DirectX::LoadFromDDSFile(filePath.c_str(), DirectX::DDS_FLAGS::DDS_FLAGS_FORCE_RGB, &metadata, scratchImage));
	}
	else if (filePath.extension() == ".tga")
	{
		ThrowCOMIfFailed(DirectX::LoadFromTGAFile(filePath.c_str(), &metadata, scratchImage));
	}
	else if (filePath.extension() == ".hdr")
	{
		ThrowCOMIfFailed(DirectX::LoadFromHDRFile(filePath.c_str(), &metadata, scratchImage));
	}
	else
	{
		ThrowCOMIfFailed(DirectX::LoadFromWICFile(filePath.c_str(), DirectX::WIC_FLAGS::WIC_FLAGS_FORCE_RGB, &metadata, scratchImage));
	}

	bool generateMips = (metadata.mipLevels > 1) ? false : GenerateMips;
	size_t mipLevels = generateMips ? static_cast<size_t>(std::floor(std::log2(std::max(metadata.width, metadata.height)))) + 1 : 1;

	if (ForceSRGB)
	{
		metadata.format = DirectX::MakeSRGB(metadata.format);
		if (!DirectX::IsSRGB(metadata.format))
		{
			CORE_WARN("Failed to convert to SRGB format");
		}
	}

	Texture::Properties textureProp{};
	textureProp.Format = metadata.format;
	textureProp.MipLevels = mipLevels;
	textureProp.Flags = D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS;
	textureProp.IsCubemap = metadata.IsCubemap();
	textureProp.InitialState = D3D12_RESOURCE_STATE_COPY_DEST;

	switch (metadata.dimension)
	{
	case DirectX::TEX_DIMENSION::TEX_DIMENSION_TEXTURE1D:
		textureProp.Type = Resource::Type::Texture1D;
		textureProp.Width = static_cast<UINT64>(metadata.width);
		textureProp.DepthOrArraySize = static_cast<UINT16>(metadata.arraySize);
		break;
	case DirectX::TEX_DIMENSION::TEX_DIMENSION_TEXTURE2D:
		textureProp.Type = Resource::Type::Texture2D;
		textureProp.Width = static_cast<UINT64>(metadata.width);
		textureProp.Height = static_cast<UINT>(metadata.height);
		textureProp.DepthOrArraySize = static_cast<UINT16>(metadata.arraySize);
		break;
	case DirectX::TEX_DIMENSION::TEX_DIMENSION_TEXTURE3D:
		textureProp.Type = Resource::Type::Texture3D;
		textureProp.Width = static_cast<UINT64>(metadata.width);
		textureProp.Height = static_cast<UINT>(metadata.height);
		textureProp.DepthOrArraySize = static_cast<UINT16>(metadata.depth);
		break;
	}

	RenderResourceHandle handle = m_RefRenderDevice.CreateTexture(textureProp);
	Texture* pTexture = m_RefRenderDevice.GetTexture(handle);

	// Create staging resource
	Microsoft::WRL::ComPtr<ID3D12Resource> stagingBuffer;
	auto pD3DDevice = m_RefRenderDevice.GetDevice().GetD3DDevice();

	UINT64 totalBytes = 0;
	pD3DDevice->GetCopyableFootprints(&pTexture->GetD3DResource()->GetDesc(), 0, pTexture->GetNumSubresources(), 0, nullptr, nullptr, nullptr, &totalBytes);

	const CD3DX12_HEAP_PROPERTIES uploadHeapProperties(D3D12_HEAP_TYPE_UPLOAD);
	ThrowCOMIfFailed(pD3DDevice->CreateCommittedResource(&uploadHeapProperties, D3D12_HEAP_FLAG_NONE,
		&CD3DX12_RESOURCE_DESC::Buffer(totalBytes), D3D12_RESOURCE_STATE_GENERIC_READ, nullptr, IID_PPV_ARGS(&stagingBuffer)));

	StagingTexture stagingTexture;
	stagingTexture.texture = Texture(stagingBuffer);
	stagingTexture.scratchImage = std::move(scratchImage);
	stagingTexture.generateMips = generateMips;
	stagingTexture.fileName = pPath;
	m_UnstagedTextures[handle] = std::move(stagingTexture);
	return handle;
}

void TexturePool::GenerateMips(RenderResourceHandle TextureHandle, RenderCommandContext* pRenderCommandContext)
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

void TexturePool::GenerateMipsUAV(RenderResourceHandle TextureHandle, RenderCommandContext* pRenderCommandContext)
{
	// Credit: https://github.com/jpvanoosten/LearningDirectX12/blob/master/DX12Lib/src/CommandList.cpp
	Descriptor descriptor = m_TextureViewAllocator.Allocate(1);
	m_RefRenderDevice.CreateSRVForTexture(TextureHandle, descriptor[0]);
	Texture* pTexture = m_RefRenderDevice.GetTexture(TextureHandle);
	auto resource = pTexture->GetD3DResource();
	auto resourceDesc = resource->GetDesc();

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
			Descriptor descriptor = m_TextureViewAllocator.Allocate(1);
			m_RefRenderDevice.CreateUAVForTexture(TextureHandle, descriptor[0], srcMip + mip + 1);

			pRenderCommandContext->TransitionBarrier(pTexture, D3D12_RESOURCE_STATE_UNORDERED_ACCESS, srcMip + mip + 1);
			pRenderCommandContext->SetUAV(RootParameters::GenerateMips::GenerateMipsRootParameter_OutMip, mip, descriptor.NumDescriptors(), descriptor);
		}

		// Pad any unused mip levels with a default UAV. Doing this keeps the DX12 runtime happy.
		if (mipCount < 4)
		{
			pRenderCommandContext->SetUAV(RootParameters::GenerateMips::GenerateMipsRootParameter_OutMip, mipCount, 4 - mipCount, m_MipMapDefaultUAV);
		}

		UINT threadGroupCountX = dstWidth + 8 - 1 / 8;
		UINT threadGroupCountY = dstHeight + 8 - 1 / 8;
		pRenderCommandContext->Dispatch(threadGroupCountX, threadGroupCountY, 1);

		pRenderCommandContext->UAVBarrier(pTexture);

		srcMip += mipCount;
	}
}

void TexturePool::GenerateMipsSRGB(RenderResourceHandle TextureHandle, RenderCommandContext* pRenderCommandContext)
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
	m_TemporaryResources.push_back(textureCopyHandle);

	Texture* pDstTexture = m_RefRenderDevice.GetTexture(textureCopyHandle);

	pRenderCommandContext->CopyResource(pDstTexture, pTexture);

	GenerateMipsUAV(textureCopyHandle, pRenderCommandContext);

	pRenderCommandContext->CopyResource(pTexture, pDstTexture);

	m_TemporaryResources.push_back(textureCopyHandle);
}

bool TexturePool::IsUAVCompatable(DXGI_FORMAT Format)
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

bool TexturePool::IsSRGB(DXGI_FORMAT Format)
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

DXGI_FORMAT TexturePool::GetUAVCompatableFormat(DXGI_FORMAT Format)
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