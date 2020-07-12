#include "pch.h"
#include "GpuTextureAllocator.h"
#include "RendererRegistry.h"

GpuTextureAllocator::GpuTextureAllocator(RenderDevice& RefRenderDevice)
	: m_RefRenderDevice(RefRenderDevice),
	m_TextureViewAllocator(&m_RefRenderDevice.GetDevice(), D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV)
{
	m_MipsNullUAVs = m_TextureViewAllocator.Allocate(4);
	for (UINT i = 0; i < 4; ++i)
	{
		D3D12_UNORDERED_ACCESS_VIEW_DESC uavDesc =
		{
			.Format = DXGI_FORMAT_R8G8B8A8_UNORM,
			.ViewDimension = D3D12_UAV_DIMENSION_TEXTURE2D,
			.Texture2D =
			{
				.MipSlice = i,
				.PlaneSlice = 0
			}
		};

		m_RefRenderDevice.GetDevice().GetD3DDevice()->CreateUnorderedAccessView(nullptr, nullptr, &uavDesc, m_MipsNullUAVs[i]);
	}

	m_EquirectangularToCubeMapNullUAVs = m_TextureViewAllocator.Allocate(5);
	for (UINT i = 0; i < 5; ++i)
	{
		D3D12_UNORDERED_ACCESS_VIEW_DESC uavDesc =
		{
			.Format = DXGI_FORMAT_R8G8B8A8_UNORM,
			.ViewDimension = D3D12_UAV_DIMENSION_TEXTURE2DARRAY,
			.Texture2DArray =
			{
				.MipSlice = i,
				.FirstArraySlice = 0,
				.ArraySize = 6,
				.PlaneSlice = 0,
			}
		};
		m_RefRenderDevice.GetDevice().GetD3DDevice()->CreateUnorderedAccessView(nullptr, nullptr, &uavDesc, m_EquirectangularToCubeMapNullUAVs[i]);
	}

	m_BRDFLUTRTV = m_RefRenderDevice.GetDevice().GetDescriptorAllocator<D3D12_RENDER_TARGET_VIEW_DESC>().Allocate(1);

	// +1 for radiance, +1 for irradiance, +1 for prefiltered
	m_SkyboxSRVs = m_TextureViewAllocator.Allocate(3);

	// Create BRDF LUT
	Texture::Properties textureProp =
	{
		.Type = Resource::Type::Texture2D,
		.Format = RendererFormats::BRDFLUTFormat,
		.Width = Resolutions::BRDFLUT,
		.Height = Resolutions::BRDFLUT,
		.DepthOrArraySize = 1,
		.MipLevels = 1,
		.Flags = D3D12_RESOURCE_FLAG_ALLOW_RENDER_TARGET,
		.IsCubemap = false,

		.pOptimizedClearValue = nullptr,
		.InitialState = D3D12_RESOURCE_STATE_RENDER_TARGET
	};
	m_BRDFLUTHandle = m_RefRenderDevice.CreateTexture(textureProp);
	m_RefRenderDevice.CreateRTV(m_BRDFLUTHandle, m_BRDFLUTRTV[0], {}, {}, {});

	Buffer::Properties bufferProp =
	{
		.SizeInBytes = 6 * Math::AlignUp<UINT64>(sizeof(RenderPassConstantsCPU), D3D12_CONSTANT_BUFFER_DATA_PLACEMENT_ALIGNMENT),
		.Stride = Math::AlignUp<UINT64>(sizeof(RenderPassConstantsCPU), D3D12_CONSTANT_BUFFER_DATA_PLACEMENT_ALIGNMENT)
	};
	m_CubemapCamerasHandle = m_RefRenderDevice.CreateBuffer(bufferProp, CPUAccessibleHeapType::Upload, nullptr);
	Buffer* pCubemapCameras = m_RefRenderDevice.GetBuffer(m_CubemapCamerasHandle);
	// Create 6 cameras for cube map
	PerspectiveCamera cameras[6];

	// Look along each coordinate axis.
	DirectX::XMVECTOR targets[6] =
	{
		DirectX::XMVectorSet(+1.0f, 0.0f, 0.0f, 0.0f), // +X
		DirectX::XMVectorSet(-1.0f, 0.0f, 0.0f, 0.0f), // -X
		DirectX::XMVectorSet(0.0f, +1.0f, 0.0f, 0.0f), // +Y
		DirectX::XMVectorSet(0.0f, -1.0f, 0.0f, 0.0f), // -Y
		DirectX::XMVectorSet(0.0f, 0.0f, +1.0f, 0.0f), // +Z
		DirectX::XMVectorSet(0.0f, 0.0f, -1.0f, 0.0f)  // -Z
	};

	// Use world up vector (0,1,0) for all directions except +Y/-Y.  In these cases, we
	// are looking down +Y or -Y, so we need a different "up" vector.
	DirectX::XMVECTOR ups[6] =
	{
		DirectX::XMVectorSet(0.0f, +1.0f, 0.0f, 0.0f), // +X
		DirectX::XMVectorSet(0.0f, +1.0f, 0.0f, 0.0f), // -X
		DirectX::XMVectorSet(0.0f, 0.0f, -1.0f, 0.0f), // +Y
		DirectX::XMVectorSet(0.0f, 0.0f, +1.0f, 0.0f), // -Y
		DirectX::XMVectorSet(0.0f, +1.0f, 0.0f, 0.0f), // +Z
		DirectX::XMVectorSet(0.0f, +1.0f, 0.0f, 0.0f)  // -Z
	};

	// Update view matrix in upload buffer
	for (UINT i = 0; i < 6; ++i)
	{
		cameras[i].SetLookAt(DirectX::XMVectorSet(0.0f, 0.0f, 0.0f, 1.0f), targets[i], ups[i]);
		cameras[i].SetLens(DirectX::XM_PIDIV2, 1.0f, 0.1f, 1000.0f);
		RenderPassConstantsCPU rpcCPU;
		XMStoreFloat4x4(&rpcCPU.ViewProjection, XMMatrixTranspose(cameras[i].ViewProjectionMatrix()));
		pCubemapCameras->Map();
		pCubemapCameras->Update<RenderPassConstantsCPU>(i, rpcCPU);
	}

	DXGI_FORMAT optionalFormats[] =
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

void GpuTextureAllocator::Stage(Scene& Scene, RenderCommandContext* pRenderCommandContext)
{
	// Cpu upload
	m_SkyboxEquirectangularHandle = LoadFromFile(Scene.Skybox.Path, false, true);
	for (auto iter = Scene.Models.begin(); iter != Scene.Models.end(); ++iter)
	{
		auto& model = (*iter);
		for (auto& material : model.Materials)
		{
			LoadMaterial(material);
		}
	}

	// Gpu copy
	for (auto iter = m_UnstagedTextures.begin(); iter != m_UnstagedTextures.end(); ++iter)
	{
		auto handle = iter->first;
		auto& stagingTexture = iter->second;

		Texture* pTexture = m_RefRenderDevice.GetTexture(handle);
		Texture* pStagingResourceTexture = &stagingTexture.texture;

		// Stage texture
		pRenderCommandContext->TransitionBarrier(pTexture, D3D12_RESOURCE_STATE_COPY_DEST);
		for (std::size_t subresourceIndex = 0; subresourceIndex < stagingTexture.numSubresources; ++subresourceIndex)
		{
			D3D12_TEXTURE_COPY_LOCATION destination;
			destination.pResource = pTexture->GetD3DResource();
			destination.Type = D3D12_TEXTURE_COPY_TYPE_SUBRESOURCE_INDEX;
			destination.SubresourceIndex = subresourceIndex;

			D3D12_TEXTURE_COPY_LOCATION source;
			source.pResource = pStagingResourceTexture->GetD3DResource();
			source.Type = D3D12_TEXTURE_COPY_TYPE_PLACED_FOOTPRINT;
			source.PlacedFootprint = stagingTexture.placedSubresourceLayouts[subresourceIndex];
			pRenderCommandContext->CopyTextureRegion(&destination, 0, 0, 0, &source, nullptr);
		}

		if (stagingTexture.generateMips)
		{
			GenerateMips(handle, pRenderCommandContext);
		}

		pRenderCommandContext->TransitionBarrier(pTexture, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);

		m_TextureHandles[stagingTexture.path] = handle;
		CORE_INFO("{} Loaded", stagingTexture.path);
	}

	// Generate cubemap for equirectangular map
	Texture* pEquirectangularMap = m_RefRenderDevice.GetTexture(m_SkyboxEquirectangularHandle);
	Texture::Properties radianceTextureProp =
	{
		.Type = pEquirectangularMap->Type,
		.Format = pEquirectangularMap->Format,
		.Width = 1024,
		.Height = 1024,
		.DepthOrArraySize = 6,
		.MipLevels = 0,
		.Flags = D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS,
		.IsCubemap = true,

		.pOptimizedClearValue = nullptr,
		.InitialState = D3D12_RESOURCE_STATE_COMMON
	};

	m_SkyboxCubemapHandle = m_RefRenderDevice.CreateTexture(radianceTextureProp);

	EquirectangularToCubemap(m_SkyboxEquirectangularHandle, m_SkyboxCubemapHandle, pRenderCommandContext);
	m_RefRenderDevice.CreateSRV(m_SkyboxCubemapHandle, m_SkyboxSRVs[0]);

	// Generate cubemap convolutions
	for (int i = 0; i < CubemapConvolution::CubemapConvolution_Count; ++i)
	{
		const DXGI_FORMAT format = i == CubemapConvolution::Irradiance ? RendererFormats::IrradianceFormat : RendererFormats::PrefilterFormat;
		const UINT64 resolution = i == CubemapConvolution::Irradiance ? Resolutions::Irradiance : Resolutions::Prefilter;
		const UINT16 numMips = static_cast<UINT16>(floor(log2(resolution))) + 1;
		const UINT num32bitValues = i == Irradiance ? sizeof(IrradianceConvolutionSetting) / 4 : sizeof(PrefilterConvolutionSetting) / 4;

		// Create cubemap texture
		Texture::Properties textureProp =
		{
			.Type = Resource::Type::Texture2D,
			.Format = format,
			.Width = resolution,
			.Height = (UINT)resolution,
			.DepthOrArraySize = 6,
			.MipLevels = numMips,
			.Flags = D3D12_RESOURCE_FLAG_ALLOW_RENDER_TARGET,
			.IsCubemap = true,

			.pOptimizedClearValue = nullptr,
			.InitialState = D3D12_RESOURCE_STATE_RENDER_TARGET
		};
		RenderResourceHandle cubemap = m_RefRenderDevice.CreateTexture(textureProp);
		UINT numDescriptorsToAllocate = numMips * 6;
		Descriptor cubemapRTVs = m_RefRenderDevice.GetDevice().GetDescriptorAllocator<D3D12_RENDER_TARGET_VIEW_DESC>().Allocate(numDescriptorsToAllocate);

		// Generate cube map
		pRenderCommandContext->SetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
		pRenderCommandContext->SetPipelineState(m_RefRenderDevice.GetGraphicsPSO(i == CubemapConvolution::Irradiance ? GraphicsPSOs::IrradiaceConvolution : GraphicsPSOs::PrefilterConvolution));
		pRenderCommandContext->SetGraphicsRootSignature(m_RefRenderDevice.GetRootSignature(i == CubemapConvolution::Irradiance ? RootSignatures::IrradiaceConvolution : RootSignatures::PrefilterConvolution));

		pRenderCommandContext->SetSRV(RootParameters::CubemapConvolution::CubemapConvolutionRootParameter_CubemapSRV, 0, 1, m_SkyboxSRVs);

		for (UINT mip = 0; mip < numMips; ++mip)
		{
			D3D12_VIEWPORT vp;
			vp.TopLeftX = vp.TopLeftY = 0.0f;
			vp.MinDepth = 0.0f;
			vp.MaxDepth = 1.0f;
			vp.Width = vp.Height = static_cast<FLOAT>(resolution * std::pow(0.5f, mip));

			D3D12_RECT sr;
			sr.left = sr.top = 0;
			sr.right = sr.bottom = resolution;

			pRenderCommandContext->SetViewports(1, &vp);
			pRenderCommandContext->SetScissorRects(1, &sr);
			for (UINT face = 0; face < 6; ++face)
			{
				UINT renderTargetDescriptorOffset = UINT(INT64(mip) * INT64(6) + INT64(face));
				m_RefRenderDevice.CreateRTV(cubemap, cubemapRTVs[renderTargetDescriptorOffset], face, mip, 1);
				pRenderCommandContext->SetRenderTargets(renderTargetDescriptorOffset, 1, cubemapRTVs, TRUE, 0, Descriptor());

				Buffer* pCubemapCameras = m_RefRenderDevice.GetBuffer(m_CubemapCamerasHandle);
				pRenderCommandContext->SetGraphicsRootCBV(RootParameters::CubemapConvolution::CubemapConvolutionRootParameter_RenderPassCBuffer, pCubemapCameras->GetBufferLocationAt(face));
				switch (i)
				{
				case Irradiance:
				{
					IrradianceConvolutionSetting icSetting;
					pRenderCommandContext->SetGraphicsRoot32BitConstants(RootParameters::CubemapConvolution::CubemapConvolutionRootParameter_Setting, num32bitValues, &icSetting, 0);
				}
				break;
				case Prefilter:
				{
					PrefilterConvolutionSetting pcSetting;
					pcSetting.Roughness = (float)mip / (float)(numMips - 1);
					pRenderCommandContext->SetGraphicsRoot32BitConstants(RootParameters::CubemapConvolution::CubemapConvolutionRootParameter_Setting, num32bitValues, &pcSetting, 0);
				}
				break;
				}
				pRenderCommandContext->DrawIndexedInstanced(Scene.Skybox.Mesh.IndexCount, 1, Scene.Skybox.Mesh.StartIndexLocation, Scene.Skybox.Mesh.BaseVertexLocation, 0);
			}
		}

		// Set cubemap
		switch (i)
		{
		case Irradiance:
		{
			m_IrradianceHandle = cubemap;
			m_RefRenderDevice.CreateSRV(m_IrradianceHandle, m_SkyboxSRVs[1]);
		}
		break;
		case Prefilter:
		{
			m_PrefilteredHandle = cubemap;
			m_RefRenderDevice.CreateSRV(m_PrefilteredHandle, m_SkyboxSRVs[2]);
		}
		break;
		}
	}

	// Generate BRDF LUT
	if (!m_Status.BRDFGenerated)
	{
		m_Status.BRDFGenerated = true;
		D3D12_VIEWPORT vp;
		vp.TopLeftX = vp.TopLeftY = 0.0f;
		vp.MinDepth = 0.0f;
		vp.MaxDepth = 1.0f;
		vp.Width = vp.Height = Resolutions::BRDFLUT;

		D3D12_RECT sr;
		sr.left = sr.top = 0;
		sr.right = sr.bottom = Resolutions::BRDFLUT;

		pRenderCommandContext->SetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
		pRenderCommandContext->SetPipelineState(m_RefRenderDevice.GetGraphicsPSO(GraphicsPSOs::BRDFIntegration));
		pRenderCommandContext->SetGraphicsRootSignature(m_RefRenderDevice.GetRootSignature(RootSignatures::BRDFIntegration));

		pRenderCommandContext->SetRenderTargets(0, 1, m_BRDFLUTRTV, TRUE, 0, Descriptor());
		pRenderCommandContext->SetViewports(1, &vp);
		pRenderCommandContext->SetScissorRects(1, &sr);
		pRenderCommandContext->DrawInstanced(3, 1, 0, 0);
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

	DirectX::ScratchImage scratchImage;
	DirectX::TexMetadata metadata;
	if (Path.extension() == ".dds")
	{
		ThrowCOMIfFailed(DirectX::LoadFromDDSFile(Path.c_str(), DirectX::DDS_FLAGS::DDS_FLAGS_FORCE_RGB, &metadata, scratchImage));
	}
	else if (Path.extension() == ".tga")
	{
		ThrowCOMIfFailed(DirectX::LoadFromTGAFile(Path.c_str(), &metadata, scratchImage));
	}
	else if (Path.extension() == ".hdr")
	{
		ThrowCOMIfFailed(DirectX::LoadFromHDRFile(Path.c_str(), &metadata, scratchImage));
	}
	else
	{
		ThrowCOMIfFailed(DirectX::LoadFromWICFile(Path.c_str(), DirectX::WIC_FLAGS::WIC_FLAGS_FORCE_RGB, &metadata, scratchImage));
	}
	// if metadata mip levels is > 1 then is already generated, other wise generate mip maps
	bool generateMips = (metadata.mipLevels > 1) ? false : GenerateMips;
	std::size_t mipLevels = generateMips ? static_cast<size_t>(std::floor(std::log2(std::max(metadata.width, metadata.height)))) + 1 : 1;

	if (ForceSRGB)
	{
		metadata.format = DirectX::MakeSRGB(metadata.format);
		if (!DirectX::IsSRGB(metadata.format))
		{
			CORE_WARN("Failed to convert to SRGB format");
		}
	}

	// Create actual texture
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
#ifdef _DEBUG
	pTexture->GetD3DResource()->SetName(Path.generic_wstring().data());
#endif

	std::vector<D3D12_SUBRESOURCE_DATA> subresources(scratchImage.GetImageCount());
	const DirectX::Image* pImages = scratchImage.GetImages();
	for (size_t i = 0; i < scratchImage.GetImageCount(); ++i)
	{
		subresources[i].RowPitch = pImages[i].rowPitch;
		subresources[i].SlicePitch = pImages[i].slicePitch;
		subresources[i].pData = pImages[i].pixels;
	}

	Microsoft::WRL::ComPtr<ID3D12Resource> stagingResource;
	const UINT NumSubresources = static_cast<UINT>(subresources.size());
	// Footprint is synonymous to layout
	std::vector<D3D12_PLACED_SUBRESOURCE_FOOTPRINT> PlacedSubresourceLayouts(NumSubresources);
	std::vector<UINT> NumRows(NumSubresources);
	std::vector<UINT64> RowSizeInBytes(NumSubresources);
	UINT64 TotalBytes = 0;

	auto pD3DDevice = m_RefRenderDevice.GetDevice().GetD3DDevice();
	pD3DDevice->GetCopyableFootprints(&pTexture->GetD3DResource()->GetDesc(), 0, NumSubresources, 0,
		PlacedSubresourceLayouts.data(), NumRows.data(), RowSizeInBytes.data(), &TotalBytes);

	pD3DDevice->CreateCommittedResource(&CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_UPLOAD), D3D12_HEAP_FLAG_NONE,
		&CD3DX12_RESOURCE_DESC::Buffer(TotalBytes), D3D12_RESOURCE_STATE_GENERIC_READ, nullptr, IID_PPV_ARGS(stagingResource.ReleaseAndGetAddressOf()));

	// CPU Upload
	BYTE* pData = nullptr;
	stagingResource->Map(0, nullptr, reinterpret_cast<void**>(&pData));
	{
		for (UINT subresourceIndex = 0; subresourceIndex < NumSubresources; ++subresourceIndex)
		{
			D3D12_MEMCPY_DEST memcpyDest;
			memcpyDest.pData = pData + PlacedSubresourceLayouts[subresourceIndex].Offset;
			memcpyDest.RowPitch = static_cast<SIZE_T>(PlacedSubresourceLayouts[subresourceIndex].Footprint.RowPitch);
			memcpyDest.SlicePitch = static_cast<SIZE_T>(PlacedSubresourceLayouts[subresourceIndex].Footprint.RowPitch) * static_cast<SIZE_T>(NumRows[subresourceIndex]);

			for (UINT z = 0; z < PlacedSubresourceLayouts[subresourceIndex].Footprint.Depth; ++z)
			{
				BYTE* pDestSlice = reinterpret_cast<BYTE*>(memcpyDest.pData) + memcpyDest.SlicePitch * static_cast<SIZE_T>(z);
				const BYTE* pSrcSlice = reinterpret_cast<const BYTE*>(subresources[subresourceIndex].pData) + subresources[subresourceIndex].SlicePitch * LONG_PTR(z);
				for (UINT y = 0; y < NumRows[subresourceIndex]; ++y)
				{
					CopyMemory(pDestSlice + memcpyDest.RowPitch * y, pSrcSlice + subresources[subresourceIndex].RowPitch * LONG_PTR(y), RowSizeInBytes[subresourceIndex]);
				}
			}
		}
	}
	stagingResource->Unmap(0, nullptr);

	StagingTexture stagingTexture;
	stagingTexture.path = Path.generic_string();
	stagingTexture.texture = Texture(stagingResource);
	stagingTexture.numSubresources = NumSubresources;
	stagingTexture.placedSubresourceLayouts = std::move(PlacedSubresourceLayouts);
	stagingTexture.mipLevels = mipLevels;
	stagingTexture.generateMips = generateMips;

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
	m_RefRenderDevice.CreateSRV(TextureHandle, descriptor[0]);
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
			m_RefRenderDevice.CreateUAV(TextureHandle, uavDescriptor[0], {}, srcMip + mip + 1);
			m_TemporaryDescriptors.push_back(uavDescriptor);

			pRenderCommandContext->TransitionBarrier(pTexture, D3D12_RESOURCE_STATE_UNORDERED_ACCESS, srcMip + mip + 1);
			pRenderCommandContext->SetUAV(RootParameters::GenerateMips::GenerateMipsRootParameter_OutMip, mip, uavDescriptor.NumDescriptors(), uavDescriptor);
		}

		// Pad any unused mip levels with a default UAV. Doing this keeps the DX12 runtime happy.
		if (mipCount < 4)
		{
			pRenderCommandContext->SetUAV(RootParameters::GenerateMips::GenerateMipsRootParameter_OutMip, mipCount, 4 - mipCount, m_MipsNullUAVs);
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
	m_RefRenderDevice.CreateSRV(EquirectangularMap, descriptor[0]);
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
			m_RefRenderDevice.CreateUAV(Cubemap, uavDescriptor[0], {}, mipSlice + mip);
			m_TemporaryDescriptors.push_back(uavDescriptor);

			pRenderCommandContext->TransitionBarrier(pCubemap, D3D12_RESOURCE_STATE_UNORDERED_ACCESS);
			pRenderCommandContext->SetUAV(RootParameters::EquirectangularToCubemap::EquirectangularToCubemapRootParameter_DstMips, mip, uavDescriptor.NumDescriptors(), uavDescriptor);
		}

		if (numMips < 5)
		{
			// Pad unused mips. This keeps DX12 runtime happy.
			pRenderCommandContext->SetUAV(RootParameters::EquirectangularToCubemap::EquirectangularToCubemapRootParameter_DstMips, panoToCubemapCB.NumMips, 5 - numMips, m_EquirectangularToCubeMapNullUAVs);
		}

		UINT threadGroupCount = Math::DivideByMultiple(panoToCubemapCB.CubemapSize, 16);
		pRenderCommandContext->Dispatch(threadGroupCount, threadGroupCount, 6);

		mipSlice += numMips;
	}
}

void GpuTextureAllocator::EquirectangularToCubemapSRGB(RenderResourceHandle EquirectangularMap, RenderResourceHandle Cubemap, RenderCommandContext* pRenderCommandContext)
{
	Texture* pCubemap = m_RefRenderDevice.GetTexture(Cubemap);

	Texture::Properties textureProp =
	{
		.Type = pCubemap->Type,
		.Format = GetUAVCompatableFormat(pCubemap->Format),
		.Width = static_cast<UINT64>(pCubemap->Width),
		.Height = static_cast<UINT>(pCubemap->Height),
		.DepthOrArraySize = static_cast<UINT16>(pCubemap->DepthOrArraySize),
		.MipLevels = pCubemap->MipLevels,
		.Flags = D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS,

		.IsCubemap = pCubemap->IsCubemap,
		.InitialState = D3D12_RESOURCE_STATE_COPY_DEST
	};

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