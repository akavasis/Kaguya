#include "pch.h"
#include "GpuTextureAllocator.h"
#include "RendererRegistry.h"

GpuTextureAllocator::GpuTextureAllocator(RenderDevice* pRenderDevice)
	: pRenderDevice(pRenderDevice)
{
	// Create BRDF LUT
	RendererReseveredTextures[BRDFLUT] = pRenderDevice->CreateDeviceTexture(DeviceResource::Type::Texture2D, [&](DeviceTextureProxy& proxy)
	{
		proxy.SetFormat(RendererFormats::BRDFLUTFormat);
		proxy.SetWidth(Resolutions::BRDFLUT);
		proxy.SetHeight(Resolutions::BRDFLUT);
		proxy.BindFlags = DeviceResource::BindFlags::RenderTarget;
		proxy.InitialState = DeviceResource::State::RenderTarget;
	});

	m_CubemapCamerasUploadBufferHandle = pRenderDevice->CreateDeviceBuffer([](DeviceBufferProxy& proxy)
	{
		proxy.SetSizeInBytes(6 * Math::AlignUp<UINT64>(sizeof(GlobalConstants), D3D12_CONSTANT_BUFFER_DATA_PLACEMENT_ALIGNMENT));
		proxy.SetStride(Math::AlignUp<UINT64>(sizeof(GlobalConstants), D3D12_CONSTANT_BUFFER_DATA_PLACEMENT_ALIGNMENT));
		proxy.SetCpuAccess(DeviceBuffer::CpuAccess::Write);
	});
	DeviceBuffer* pCubemapCameras = pRenderDevice->GetBuffer(m_CubemapCamerasUploadBufferHandle);
	pCubemapCameras->Map();

	// TODO: MOVE THIS INTO CONSTANTS IN HLSL CODE
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
		GlobalConstants rpcCPU;
		XMStoreFloat4x4(&rpcCPU.ViewProjection, XMMatrixTranspose(cameras[i].ViewProjectionMatrix()));
		pCubemapCameras->Update<GlobalConstants>(i, rpcCPU);
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

		ThrowCOMIfFailed(pRenderDevice->Device.GetD3DDevice()->CheckFeatureSupport(
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

void GpuTextureAllocator::Stage(Scene& Scene, RenderContext& RenderContext)
{
	// Generate BRDF LUT
	if (!Status::BRDFGenerated)
	{
		Status::BRDFGenerated = true;

		D3D12_VIEWPORT vp;
		vp.TopLeftX = vp.TopLeftY = 0.0f;
		vp.MinDepth = 0.0f;
		vp.MaxDepth = 1.0f;
		vp.Width = vp.Height = Resolutions::BRDFLUT;

		D3D12_RECT sr;
		sr.left = sr.top = 0;
		sr.right = sr.bottom = Resolutions::BRDFLUT;

		RenderContext->TransitionBarrier(pRenderDevice->GetTexture(RendererReseveredTextures[BRDFLUT]), DeviceResource::State::RenderTarget);

		RenderContext.SetPipelineState(GraphicsPSOs::BRDFIntegration);
		RenderContext->SetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);

		pRenderDevice->CreateRenderTargetView(RendererReseveredTextures[BRDFLUT]);

		Descriptor RTV = pRenderDevice->GetRenderTargetView(RendererReseveredTextures[BRDFLUT]);

		RenderContext->SetRenderTargets(1, &RTV, TRUE, Descriptor());
		RenderContext->SetViewports(1, &vp);
		RenderContext->SetScissorRects(1, &sr);
		RenderContext->DrawInstanced(3, 1, 0, 0);

		RenderContext.TransitionBarrier(RendererReseveredTextures[BRDFLUT], DeviceResource::State::PixelShaderResource | DeviceResource::State::NonPixelShaderResource);
	}

	// Generate skybox first
	RendererReseveredTextures[SkyboxEquirectangularMap] = LoadFromFile(Scene.Skybox.Path, false, true);
	auto& stagingTexture = m_UnstagedTextures[RendererReseveredTextures[SkyboxEquirectangularMap]];
	StageTexture(RendererReseveredTextures[SkyboxEquirectangularMap], stagingTexture, RenderContext);

	// Generate cubemap for equirectangular map
	DeviceTexture* pEquirectangularMap = pRenderDevice->GetTexture(RendererReseveredTextures[SkyboxEquirectangularMap]);
	RendererReseveredTextures[SkyboxCubemap] = pRenderDevice->CreateDeviceTexture(DeviceResource::Type::TextureCube, [&](DeviceTextureProxy& proxy)
	{
		proxy.SetFormat(pEquirectangularMap->GetFormat());
		proxy.SetWidth(1024);
		proxy.SetHeight(1024);
		proxy.SetMipLevels(0);
		proxy.BindFlags = DeviceResource::BindFlags::UnorderedAccess;
		proxy.InitialState = DeviceResource::State::Common;
	});

	EquirectangularToCubemap(RendererReseveredTextures[SkyboxEquirectangularMap], RendererReseveredTextures[SkyboxCubemap], RenderContext);
	pRenderDevice->CreateShaderResourceView(RendererReseveredTextures[SkyboxCubemap]);

	//// Create temporary srv for the cubemap and generate convolutions
	//pRenderDevice->CreateSRV(RendererReseveredTextures[SkyboxCubemap]);
	//Descriptor skyboxSRV = pRenderDevice->GetSRV(RendererReseveredTextures[SkyboxCubemap]);
	//pCommandContext->TransitionBarrier(pRenderDevice->GetTexture(RendererReseveredTextures[SkyboxCubemap]), Resource::State::PixelShaderResource | Resource::State::NonPixelShaderResource);

	//// Generate cubemap convolutions
	//for (int i = 0; i < CubemapConvolution::NumCubemapConvolutions; ++i)
	//{
	//	const DXGI_FORMAT format = i == CubemapConvolution::Irradiance ? RendererFormats::IrradianceFormat : RendererFormats::PrefilterFormat;
	//	const UINT64 resolution = i == CubemapConvolution::Irradiance ? Resolutions::Irradiance : Resolutions::Prefilter;
	//	const UINT16 numMips = static_cast<UINT16>(floor(log2(resolution))) + 1;

	//	// Create cubemap texture
	//	RenderResourceHandle cubemap = pRenderDevice->CreateTexture(Resource::Type::TextureCube, [&](TextureProxy& proxy)
	//	{
	//		proxy.SetFormat(format);
	//		proxy.SetWidth(resolution);
	//		proxy.SetHeight(resolution);
	//		proxy.SetMipLevels(numMips);
	//		proxy.BindFlags = Resource::BindFlags::RenderTarget;
	//		proxy.InitialState = Resource::State::RenderTarget;
	//	});

	//	// Generate cube map
	//	pCommandContext->SetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
	//	pCommandContext->SetPipelineState(pRenderDevice->GetGraphicsPSO(i == CubemapConvolution::Irradiance ? GraphicsPSOs::ConvolutionIrradiace : GraphicsPSOs::ConvolutionPrefilter));
	//	pCommandContext->SetGraphicsRootSignature(pRenderDevice->GetRootSignature(i == CubemapConvolution::Irradiance ? RootSignatures::ConvolutionIrradiace : RootSignatures::ConvolutionPrefilter));

	//	pCommandContext->SetGraphicsRootDescriptorTable(RootParameters::CubemapConvolution::CubemapSRV, skyboxSRV.GPUHandle);

	//	for (UINT mip = 0; mip < numMips; ++mip)
	//	{
	//		D3D12_VIEWPORT vp;
	//		vp.TopLeftX = vp.TopLeftY = 0.0f;
	//		vp.MinDepth = 0.0f;
	//		vp.MaxDepth = 1.0f;
	//		vp.Width = vp.Height = static_cast<FLOAT>(resolution * std::pow(0.5f, mip));

	//		D3D12_RECT sr;
	//		sr.left = sr.top = 0;
	//		sr.right = sr.bottom = resolution;

	//		pCommandContext->SetViewports(1, &vp);
	//		pCommandContext->SetScissorRects(1, &sr);
	//		for (UINT face = 0; face < 6; ++face)
	//		{
	//			UINT renderTargetDescriptorOffset = UINT(INT64(mip) * INT64(6) + INT64(face));
	//			pRenderDevice->CreateRTV(cubemap, face, mip, 1);
	//			Descriptor RTV = pRenderDevice->GetRTV(cubemap);
	//			pCommandContext->SetRenderTargets(1, RTV, TRUE, Descriptor());

	//			Buffer* pCubemapCameras = pRenderDevice->GetBuffer(m_CubemapCamerasUploadBufferHandle);
	//			pCommandContext->SetGraphicsRootConstantBufferView(RootParameters::CubemapConvolution::RenderPassCBuffer, pCubemapCameras->GetGpuVirtualAddressAt(face));
	//			switch (i)
	//			{
	//			case Irradiance:
	//			{
	//				ConvolutionIrradianceSetting icSetting;
	//				pCommandContext->SetGraphicsRoot32BitConstants(RootParameters::CubemapConvolution::Setting, sizeof(icSetting) / 4, &icSetting, 0);
	//			}
	//			break;
	//			case Prefilter:
	//			{
	//				ConvolutionPrefilterSetting pcSetting;
	//				pcSetting.Roughness = (float)mip / (float)(numMips - 1);
	//				pCommandContext->SetGraphicsRoot32BitConstants(RootParameters::CubemapConvolution::Setting, sizeof(pcSetting) / 4, &pcSetting, 0);
	//			}
	//			break;
	//			}
	//			pCommandContext->DrawIndexedInstanced(Scene.Skybox.Mesh.IndexCount, 1, Scene.Skybox.Mesh.StartIndexLocation, Scene.Skybox.Mesh.BaseVertexLocation, 0);
	//		}
	//	}

	//	// Set cubemap
	//	switch (i)
	//	{
	//	case Irradiance: RendererReseveredTextures[SkyboxIrradianceCubemap] = cubemap; break;
	//	case Prefilter: RendererReseveredTextures[SkyboxPrefilteredCubemap] = cubemap; break;
	//	}
	//}

	for (auto& material : Scene.Materials)
	{
		LoadMaterial(material);
	}

	// Gpu copy
	for (auto& [handle, stagingTexture] : m_UnstagedTextures)
	{
		// Skybox is already staged, dont need to stage again
		if (handle == RendererReseveredTextures[SkyboxEquirectangularMap])
			continue;

		StageTexture(handle, stagingTexture, RenderContext);
	}
}

void GpuTextureAllocator::DisposeResources()
{
	m_UnstagedTextures.clear();

	for (auto& handle : m_TemporaryResources)
	{
		pRenderDevice->Destroy(&handle);
	}
	m_TemporaryResources.clear();
}

RenderResourceHandle GpuTextureAllocator::LoadFromFile(const std::filesystem::path& Path, bool ForceSRGB, bool GenerateMips)
{
	if (auto iter = TextureHandles.find(Path.generic_string());
		iter != TextureHandles.end())
	{
		return iter->second;
	}

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
	std::size_t mipLevels = generateMips ? static_cast<size_t>(std::floor(std::log2(std::max(metadata.width, metadata.height)))) + 1 : metadata.mipLevels;

	if (ForceSRGB)
	{
		metadata.format = DirectX::MakeSRGB(metadata.format);
		if (!DirectX::IsSRGB(metadata.format))
		{
			CORE_WARN("Failed to convert to SRGB format");
		}
	}

	// Create actual texture
	RenderResourceHandle handle;

	switch (metadata.dimension)
	{
	case DirectX::TEX_DIMENSION::TEX_DIMENSION_TEXTURE1D:
	{
		handle = pRenderDevice->CreateDeviceTexture(DeviceResource::Type::Texture1D, [&](DeviceTextureProxy& proxy)
		{
			proxy.SetFormat(metadata.format);
			proxy.SetWidth(static_cast<UINT64>(metadata.width));
			proxy.SetDepthOrArraySize(static_cast<UINT16>(metadata.arraySize));
			proxy.SetMipLevels(mipLevels);
			proxy.BindFlags = DeviceResource::BindFlags::UnorderedAccess;
			proxy.InitialState = DeviceResource::State::CopyDest;
		});
	}
	break;

	case DirectX::TEX_DIMENSION::TEX_DIMENSION_TEXTURE2D:
	{
		handle = pRenderDevice->CreateDeviceTexture(DeviceResource::Type::Texture2D, [&](DeviceTextureProxy& proxy)
		{
			proxy.SetFormat(metadata.format);
			proxy.SetWidth(static_cast<UINT64>(metadata.width));
			proxy.SetHeight(static_cast<UINT>(metadata.height));
			proxy.SetDepthOrArraySize(static_cast<UINT16>(metadata.arraySize));
			proxy.SetMipLevels(mipLevels);
			proxy.BindFlags = DeviceResource::BindFlags::UnorderedAccess;
			proxy.InitialState = DeviceResource::State::CopyDest;
		});
	}
	break;

	case DirectX::TEX_DIMENSION::TEX_DIMENSION_TEXTURE3D:
	{
		handle = pRenderDevice->CreateDeviceTexture(DeviceResource::Type::Texture3D, [&](DeviceTextureProxy& proxy)
		{
			proxy.SetFormat(metadata.format);
			proxy.SetWidth(static_cast<UINT64>(metadata.width));
			proxy.SetHeight(static_cast<UINT>(metadata.height));
			proxy.SetDepthOrArraySize(static_cast<UINT16>(metadata.depth));
			proxy.SetMipLevels(mipLevels);
			proxy.BindFlags = DeviceResource::BindFlags::UnorderedAccess;
			proxy.InitialState = DeviceResource::State::CopyDest;
		});
	}
	break;

	default:
	{
		throw std::logic_error("Unknown TEX_DIMENSION");
	}
	}

	DeviceTexture* pTexture = pRenderDevice->GetTexture(handle);
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
	std::vector<D3D12_PLACED_SUBRESOURCE_FOOTPRINT> placedSubresourceLayouts(NumSubresources);
	std::vector<UINT> numRows(NumSubresources);
	std::vector<UINT64> rowSizeInBytes(NumSubresources);
	UINT64 totalBytes = 0;

	auto pD3DDevice = pRenderDevice->Device.GetD3DDevice();
	pD3DDevice->GetCopyableFootprints(&pTexture->GetD3DResource()->GetDesc(), 0, NumSubresources, 0,
		placedSubresourceLayouts.data(), numRows.data(), rowSizeInBytes.data(), &totalBytes);

	pD3DDevice->CreateCommittedResource(&CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_UPLOAD), D3D12_HEAP_FLAG_NONE,
		&CD3DX12_RESOURCE_DESC::Buffer(totalBytes), D3D12_RESOURCE_STATE_GENERIC_READ, nullptr, IID_PPV_ARGS(stagingResource.ReleaseAndGetAddressOf()));

#ifdef _DEBUG
	std::wstring stagingName = L"Staging Resource: " + Path.generic_wstring();
	stagingResource->SetName(stagingName.data());
#endif

	// CPU Upload
	BYTE* pData = nullptr;
	stagingResource->Map(0, nullptr, reinterpret_cast<void**>(&pData));
	{
		for (UINT subresourceIndex = 0; subresourceIndex < NumSubresources; ++subresourceIndex)
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
	stagingResource->Unmap(0, nullptr);

	StagingTexture stagingTexture;
	stagingTexture.path = Path.generic_string();
	stagingTexture.texture = DeviceTexture(stagingResource);
	stagingTexture.numSubresources = NumSubresources;
	stagingTexture.placedSubresourceLayouts = std::move(placedSubresourceLayouts);
	stagingTexture.mipLevels = mipLevels;
	stagingTexture.generateMips = generateMips;
	stagingTexture.isMasked = metadata.GetAlphaMode() != DirectX::TEX_ALPHA_MODE_OPAQUE;

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
			bool srgb = i == AlbedoIdx || i == EmissiveIdx;

			if (!Material.Textures[i].Path.empty())
			{
				LoadFromFile(Material.Textures[i].Path, srgb, true);
			}
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

void GpuTextureAllocator::StageTexture(RenderResourceHandle TextureHandle, StagingTexture& StagingTexture, RenderContext& RenderContext)
{
	DeviceTexture* pTexture = pRenderDevice->GetTexture(TextureHandle);
	DeviceTexture* pStagingResourceTexture = &StagingTexture.texture;

	// Stage texture
	RenderContext->TransitionBarrier(pTexture, DeviceResource::State::CopyDest);
	for (std::size_t subresourceIndex = 0; subresourceIndex < StagingTexture.numSubresources; ++subresourceIndex)
	{
		D3D12_TEXTURE_COPY_LOCATION destination;
		destination.pResource = pTexture->GetD3DResource();
		destination.Type = D3D12_TEXTURE_COPY_TYPE_SUBRESOURCE_INDEX;
		destination.SubresourceIndex = subresourceIndex;

		D3D12_TEXTURE_COPY_LOCATION source;
		source.pResource = pStagingResourceTexture->GetD3DResource();
		source.Type = D3D12_TEXTURE_COPY_TYPE_PLACED_FOOTPRINT;
		source.PlacedFootprint = StagingTexture.placedSubresourceLayouts[subresourceIndex];
		RenderContext->CopyTextureRegion(&destination, 0, 0, 0, &source, nullptr);
	}

	if (StagingTexture.generateMips)
	{
		GenerateMips(TextureHandle, RenderContext);
	}

	RenderContext->TransitionBarrier(pTexture, DeviceResource::State::PixelShaderResource | DeviceResource::State::NonPixelShaderResource);

	TextureHandles[StagingTexture.path] = TextureHandle;
	CORE_INFO("{} Loaded", StagingTexture.path);
}

void GpuTextureAllocator::GenerateMips(RenderResourceHandle TextureHandle, RenderContext& RenderContext)
{
	DeviceTexture* pTexture = pRenderDevice->GetTexture(TextureHandle);
	if (IsUAVCompatable(pTexture->GetFormat()))
	{
		GenerateMipsUAV(TextureHandle, RenderContext);
	}
	else if (IsSRGB(pTexture->GetFormat()))
	{
		GenerateMipsSRGB(TextureHandle, RenderContext);
	}
}

void GpuTextureAllocator::GenerateMipsUAV(RenderResourceHandle TextureHandle, RenderContext& RenderContext)
{
	// Credit: https://github.com/jpvanoosten/LearningDirectX12/blob/master/DX12Lib/src/CommandList.cpp
	pRenderDevice->CreateShaderResourceView(TextureHandle);

	DeviceTexture* pTexture = pRenderDevice->GetTexture(TextureHandle);

	RenderContext.SetPipelineState(ComputePSOs::GenerateMips);

	GenerateMipsData generateMipsData;
	generateMipsData.IsSRGB = DirectX::IsSRGB(pTexture->GetFormat());

	for (uint32_t srcMip = 0; srcMip < pTexture->GetMipLevels() - 1u; )
	{
		uint64_t srcWidth = pTexture->GetWidth() >> srcMip;
		uint32_t srcHeight = pTexture->GetHeight() >> srcMip;
		uint32_t dstWidth = static_cast<uint32_t>(srcWidth >> 1);
		uint32_t dstHeight = srcHeight >> 1;

		// Determine the switch case to use in CS
		// 0b00(0): Both width and height are even.
		// 0b01(1): Width is odd, height is even.
		// 0b10(2): Width is even, height is odd.
		// 0b11(3): Both width and height are odd.
		generateMipsData.SrcDimension = (srcHeight & 1) << 1 | (srcWidth & 1);

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

		generateMipsData.SrcMipLevel = srcMip;
		generateMipsData.NumMipLevels = mipCount;
		generateMipsData.TexelSize.x = 1.0f / (float)dstWidth;
		generateMipsData.TexelSize.y = 1.0f / (float)dstHeight;

		for (uint32_t mip = 0; mip < mipCount; ++mip)
		{
			pRenderDevice->CreateUnorderedAccessView(TextureHandle, {}, srcMip + mip + 1);
		}

		generateMipsData.InputIndex = pRenderDevice->GetShaderResourceView(TextureHandle).HeapIndex;

		int outputIndices[4] = { -1, -1, -1, -1 };
		for (uint32_t mip = 0; mip < mipCount; ++mip)
		{
			outputIndices[mip] = pRenderDevice->GetUnorderedAccessView(TextureHandle, {}, srcMip + mip + 1).HeapIndex;
		}

		generateMipsData.Output1Index = outputIndices[0];
		generateMipsData.Output2Index = outputIndices[1];
		generateMipsData.Output3Index = outputIndices[2];
		generateMipsData.Output4Index = outputIndices[3];

		RenderContext->TransitionBarrier(pTexture, DeviceResource::State::NonPixelShaderResource, srcMip);
		for (uint32_t mip = 0; mip < mipCount; ++mip)
		{
			RenderContext->TransitionBarrier(pTexture, DeviceResource::State::UnorderedAccess, srcMip + mip + 1);
		}

		RenderContext->SetComputeRoot32BitConstants(0, sizeof(GenerateMipsData) / 4, &generateMipsData, 0);

		RenderContext->Dispatch2D(dstWidth, dstHeight, 8, 8);

		RenderContext->UAVBarrier(pTexture);
		RenderContext->FlushResourceBarriers();

		srcMip += mipCount;
	}
}

void GpuTextureAllocator::GenerateMipsSRGB(RenderResourceHandle TextureHandle, RenderContext& RenderContext)
{
	DeviceTexture* pTexture = pRenderDevice->GetTexture(TextureHandle);

	RenderResourceHandle textureCopyHandle = pRenderDevice->CreateDeviceTexture(pTexture->GetType(), [&](DeviceTextureProxy& proxy)
	{
		proxy.SetFormat(pTexture->GetFormat());
		proxy.SetWidth(pTexture->GetWidth());
		proxy.SetHeight(pTexture->GetHeight());
		proxy.SetDepthOrArraySize(pTexture->GetDepthOrArraySize());
		proxy.SetMipLevels(pTexture->GetMipLevels());
		proxy.BindFlags = DeviceResource::BindFlags::UnorderedAccess;
		proxy.InitialState = DeviceResource::State::CopyDest;
	});

	DeviceTexture* pDstTexture = pRenderDevice->GetTexture(textureCopyHandle);

	RenderContext->CopyResource(pDstTexture, pTexture);

	GenerateMipsUAV(textureCopyHandle, RenderContext);

	RenderContext->CopyResource(pTexture, pDstTexture);

	m_TemporaryResources.push_back(textureCopyHandle);
}

void GpuTextureAllocator::EquirectangularToCubemap(RenderResourceHandle EquirectangularMap, RenderResourceHandle Cubemap, RenderContext& RenderContext)
{
	DeviceTexture* pCubemap = pRenderDevice->GetTexture(Cubemap);
	if (IsUAVCompatable(pCubemap->GetFormat()))
	{
		EquirectangularToCubemapUAV(EquirectangularMap, Cubemap, RenderContext);
	}
	else if (IsSRGB(pCubemap->GetFormat()))
	{
		EquirectangularToCubemapSRGB(EquirectangularMap, Cubemap, RenderContext);
	}
}

void GpuTextureAllocator::EquirectangularToCubemapUAV(RenderResourceHandle EquirectangularMap, RenderResourceHandle Cubemap, RenderContext& RenderContext)
{
	pRenderDevice->CreateShaderResourceView(EquirectangularMap);

	DeviceTexture* pEquirectangularMap = pRenderDevice->GetTexture(EquirectangularMap);
	DeviceTexture* pCubemap = pRenderDevice->GetTexture(Cubemap);
	auto resourceDesc = pCubemap->GetD3DResource()->GetDesc();

	RenderContext.SetPipelineState(ComputePSOs::EquirectangularToCubemap);

	EquirectangularToCubemapData equirectangylarToCubemapData;

	for (uint32_t mipSlice = 0; mipSlice < resourceDesc.MipLevels; )
	{
		// Maximum number of mips to generate per pass is 5.
		uint32_t numMips = std::min<uint32_t>(5, resourceDesc.MipLevels - mipSlice);

		equirectangylarToCubemapData.FirstMip = mipSlice;
		equirectangylarToCubemapData.CubemapSize = std::max<uint32_t>(static_cast<uint32_t>(resourceDesc.Width), resourceDesc.Height) >> mipSlice;
		equirectangylarToCubemapData.NumMips = numMips;

		for (uint32_t mip = 0; mip < numMips; ++mip)
		{
			pRenderDevice->CreateUnorderedAccessView(Cubemap, {}, mipSlice + mip);
		}

		int outputIndices[5] = { -1, -1, -1, -1, -1 };
		for (uint32_t mip = 0; mip < numMips; ++mip)
		{
			outputIndices[mip] = pRenderDevice->GetUnorderedAccessView(Cubemap, {}, mipSlice + mip).HeapIndex;
		}

		equirectangylarToCubemapData.InputIndex = pRenderDevice->GetShaderResourceView(EquirectangularMap).HeapIndex;

		equirectangylarToCubemapData.Output1Index = outputIndices[0];
		equirectangylarToCubemapData.Output2Index = outputIndices[1];
		equirectangylarToCubemapData.Output3Index = outputIndices[2];
		equirectangylarToCubemapData.Output4Index = outputIndices[3];
		equirectangylarToCubemapData.Output5Index = outputIndices[4];

		RenderContext->TransitionBarrier(pEquirectangularMap, DeviceResource::State::NonPixelShaderResource);
		for (uint32_t mip = 0; mip < numMips; ++mip)
		{
			RenderContext->TransitionBarrier(pCubemap, DeviceResource::State::UnorderedAccess);
		}

		RenderContext->SetComputeRoot32BitConstants(0, sizeof(EquirectangularToCubemapData) / 4, &equirectangylarToCubemapData, 0);

		UINT threadGroupCount = Math::RoundUpAndDivide(equirectangylarToCubemapData.CubemapSize, 16);
		RenderContext->Dispatch(threadGroupCount, threadGroupCount, 6);

		RenderContext->UAVBarrier(pCubemap);

		mipSlice += numMips;
	}
}

void GpuTextureAllocator::EquirectangularToCubemapSRGB(RenderResourceHandle EquirectangularMap, RenderResourceHandle Cubemap, RenderContext& RenderContext)
{
	DeviceTexture* pCubemap = pRenderDevice->GetTexture(Cubemap);

	RenderResourceHandle textureCopyHandle = pRenderDevice->CreateDeviceTexture(pCubemap->GetType(), [&](DeviceTextureProxy& proxy)
	{
		proxy.SetFormat(pCubemap->GetFormat());
		proxy.SetWidth(pCubemap->GetWidth());
		proxy.SetHeight(pCubemap->GetHeight());
		proxy.SetDepthOrArraySize(pCubemap->GetDepthOrArraySize());
		proxy.SetMipLevels(pCubemap->GetMipLevels());
		proxy.BindFlags = DeviceResource::BindFlags::UnorderedAccess;
		proxy.InitialState = DeviceResource::State::CopyDest;
	});

	DeviceTexture* pDstTexture = pRenderDevice->GetTexture(textureCopyHandle);

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