#include "pch.h"
#include "TexturePool.h"

void TexturePool::Stage(RenderCommandContext* pRenderCommandContext)
{
	for (auto iter = m_UnstagedTextures.begin(); iter != m_UnstagedTextures.end(); ++iter)
	{
		
	}
}

RenderResourceHandle TexturePool::LoadFromFile(const char* pPath, bool ForceSRGB, bool GenerateMips)
{
	std::filesystem::path filePath = m_ExecutableFolderPath / pPath;
	if (!std::filesystem::exists(filePath))
	{
		throw std::exception("File Not Found");
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
		metadata.format = DirectX::MakeSRGB(metadata.format);

	Texture::Properties textureProp{};
	textureProp.Format = metadata.format;
	textureProp.MipLevels = mipLevels;
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
	const Texture* pTexture = m_RefRenderDevice.GetTexture(handle);

	// Create staging resource
	Microsoft::WRL::ComPtr<ID3D12Resource> stagingBuffer;

	std::vector<D3D12_SUBRESOURCE_DATA> subresources(scratchImage.GetImageCount());
	const DirectX::Image* pImages = scratchImage.GetImages();
	for (size_t i = 0; i < scratchImage.GetImageCount(); ++i)
	{
		subresources[i].RowPitch = pImages[i].rowPitch;
		subresources[i].SlicePitch = pImages[i].slicePitch;
		subresources[i].pData = pImages[i].pixels;
	}

	const UINT numSubresources = static_cast<UINT>(subresources.size());
	// Footprint is synonymous to layout
	std::vector<D3D12_PLACED_SUBRESOURCE_FOOTPRINT> placedSubresourceLayouts(numSubresources);
	std::vector<UINT> numRows(numSubresources);
	std::vector<UINT64> rowSizeInBytes(numSubresources);
	UINT64 totalBytes = 0;

	auto pD3DDevice = m_RefRenderDevice.GetDevice().GetD3DDevice();
	{
		pD3DDevice->GetCopyableFootprints(&pTexture->GetD3DResource()->GetDesc(), 0, numSubresources, 0,
			placedSubresourceLayouts.data(), numRows.data(), rowSizeInBytes.data(), &totalBytes);

		const CD3DX12_HEAP_PROPERTIES uploadHeapProperties(D3D12_HEAP_TYPE_UPLOAD);
		ThrowCOMIfFailed(pD3DDevice->CreateCommittedResource(&uploadHeapProperties, D3D12_HEAP_FLAG_NONE,
			&CD3DX12_RESOURCE_DESC::Buffer(totalBytes), D3D12_RESOURCE_STATE_GENERIC_READ, nullptr, IID_PPV_ARGS(&stagingBuffer)));
	}

	// Upload cpu data
	BYTE* pData = nullptr;
	stagingBuffer->Map(0, nullptr, reinterpret_cast<void**>(&pData));
	{
		for (UINT subresourceIndex = 0; subresourceIndex < numSubresources; ++subresourceIndex)
		{
			D3D12_MEMCPY_DEST memcpyDest;
			memcpyDest.pData = pData + placedSubresourceLayouts[subresourceIndex].Offset;
			memcpyDest.RowPitch = static_cast<SIZE_T>(placedSubresourceLayouts[subresourceIndex].Footprint.RowPitch);
			memcpyDest.SlicePitch = static_cast<SIZE_T>(placedSubresourceLayouts[subresourceIndex].Footprint.RowPitch) * static_cast<SIZE_T>(numRows[subresourceIndex]);

			for (UINT z = 0; z < placedSubresourceLayouts[subresourceIndex].Footprint.Depth; ++z)
			{
				BYTE* pDestSlice = reinterpret_cast<BYTE*>(memcpyDest.pData) + memcpyDest.SlicePitch * static_cast<SIZE_T>(z);
				const BYTE* pSrcSlice = reinterpret_cast<const BYTE*>(subresources[subresourceIndex].pData) + subresources[subresourceIndex].SlicePitch * LONG_PTR(z);
				for (UINT y = 0; y < numRows[subresourceIndex]; ++y)
				{
					CopyMemory(pDestSlice + memcpyDest.RowPitch * y, pSrcSlice + subresources[subresourceIndex].RowPitch * LONG_PTR(y), rowSizeInBytes[subresourceIndex]);
				}
			}
		}
	}
	stagingBuffer->Unmap(0, nullptr);

	StagingTexture stagingTexture;
	stagingTexture.texture = Texture(stagingBuffer);
	stagingTexture.generateMips = generateMips;
	m_UnstagedTextures[handle] = std::move(stagingTexture);
	return handle;
}

void TexturePool::GenerateMips(RenderResourceHandle Texture, RenderCommandContext* pRenderCommandContext)
{
	
}