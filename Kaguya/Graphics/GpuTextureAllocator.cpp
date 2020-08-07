#include "pch.h"
#include "GpuTextureAllocator.h"
#include "RendererRegistry.h"

size_t GpuTextureAllocator::TextureStorage::Size() const
{
	// Size should be same, but this is a insanity check
	assert(TextureHandles.size() == TextureIndices.size());
	return TextureHandles.size();
}

void GpuTextureAllocator::TextureStorage::AddTexture(const std::string& Path, RenderResourceHandle AssociatedHandle, std::size_t GpuTextureIndex)
{
	TextureHandles[Path] = AssociatedHandle;
	TextureIndices[Path] = GpuTextureIndex;
}

void GpuTextureAllocator::TextureStorage::RemoveTexture(const std::string& Path)
{
	TextureHandles.erase(Path);
	TextureIndices.erase(Path);
}

std::pair<RenderResourceHandle, size_t> GpuTextureAllocator::TextureStorage::GetTexture(const std::string& Path) const
{
	RenderResourceHandle renderResourceHandle = {};
	size_t textureIndex = -1;

	auto handleIter = TextureHandles.find(Path);
	auto indexIter = TextureIndices.find(Path);

	if (handleIter == TextureHandles.end() && indexIter == TextureIndices.end())
	{
		CORE_WARN("Could not find texture with path: {}", Path);
	}
	else
	{
		renderResourceHandle = handleIter->second;
		textureIndex = indexIter->second;
	}

	return { renderResourceHandle, textureIndex };
}

GpuTextureAllocator::GpuTextureAllocator(RenderDevice* pRenderDevice, size_t NumMaterials)
	: pRenderDevice(pRenderDevice),
	m_CBSRUADescriptorHeap(&pRenderDevice->GetDevice(), NumDescriptorsPerRange, NumDescriptorsPerRange, NumDescriptorsPerRange, true)
{
	m_RTV = pRenderDevice->GetDescriptorAllocator().AllocateRenderTargetDescriptors(1);

	// Create BRDF LUT
	AssetTextures[BRDFLUT] = pRenderDevice->CreateTexture(Resource::Type::Texture2D, [&](TextureProxy& proxy)
	{
		proxy.SetFormat(RendererFormats::BRDFLUTFormat);
		proxy.SetWidth(Resolutions::BRDFLUT);
		proxy.SetHeight(Resolutions::BRDFLUT);
		proxy.BindFlags = Resource::BindFlags::RenderTarget;
		proxy.InitialState = Resource::State::RenderTarget;
	});

	m_CubemapCamerasUploadBufferHandle = pRenderDevice->CreateBuffer([](BufferProxy& proxy)
	{
		proxy.SetSizeInBytes(6 * Math::AlignUp<UINT64>(sizeof(RenderPassConstants), D3D12_CONSTANT_BUFFER_DATA_PLACEMENT_ALIGNMENT));
		proxy.SetStride(Math::AlignUp<UINT64>(sizeof(RenderPassConstants), D3D12_CONSTANT_BUFFER_DATA_PLACEMENT_ALIGNMENT));
		proxy.SetCpuAccess(Buffer::CpuAccess::Write);
	});
	Buffer* pCubemapCameras = pRenderDevice->GetBuffer(m_CubemapCamerasUploadBufferHandle);
	pCubemapCameras->Map();

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
		RenderPassConstants rpcCPU;
		XMStoreFloat4x4(&rpcCPU.ViewProjection, XMMatrixTranspose(cameras[i].ViewProjectionMatrix()));
		pCubemapCameras->Update<RenderPassConstants>(i, rpcCPU);
	}

	m_MaterialTextureIndicesStructuredBufferHandle = pRenderDevice->CreateBuffer([NumMaterials](BufferProxy& proxy)
	{
		proxy.SetSizeInBytes(NumMaterials * sizeof(MaterialTextureIndices));
		proxy.SetStride(sizeof(MaterialTextureIndices));
		proxy.SetCpuAccess(Buffer::CpuAccess::Write);
	});
	m_pMaterialTextureIndicesStructuredBuffer = pRenderDevice->GetBuffer(m_MaterialTextureIndicesStructuredBufferHandle);
	m_pMaterialTextureIndicesStructuredBuffer->Map();

	m_MaterialTexturePropertiesStructuredBufferHandle = pRenderDevice->CreateBuffer([NumMaterials](BufferProxy& proxy)
	{
		proxy.SetSizeInBytes(NumMaterials * sizeof(MaterialTextureProperties));
		proxy.SetStride(sizeof(MaterialTextureProperties));
		proxy.SetCpuAccess(Buffer::CpuAccess::Write);
	});
	m_pMaterialTexturePropertiesStructuredBuffer = pRenderDevice->GetBuffer(m_MaterialTexturePropertiesStructuredBufferHandle);
	m_pMaterialTexturePropertiesStructuredBuffer->Map();

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

		ThrowCOMIfFailed(pRenderDevice->GetDevice().GetD3DDevice()->CheckFeatureSupport(
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

	m_NumTextures = NumAssetTextures;
}

void GpuTextureAllocator::Stage(Scene& Scene, CommandContext* pCommandContext)
{
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

		pCommandContext->TransitionBarrier(pRenderDevice->GetTexture(AssetTextures[BRDFLUT]), Resource::State::RenderTarget);

		pCommandContext->SetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
		pCommandContext->SetPipelineState(pRenderDevice->GetGraphicsPSO(GraphicsPSOs::BRDFIntegration));
		pCommandContext->SetGraphicsRootSignature(pRenderDevice->GetRootSignature(RootSignatures::BRDFIntegration));

		pRenderDevice->CreateRTV(AssetTextures[BRDFLUT], m_RTV[0], {}, {}, {});

		pCommandContext->SetRenderTargets(1, m_RTV[0], TRUE, Descriptor());
		pCommandContext->SetViewports(1, &vp);
		pCommandContext->SetScissorRects(1, &sr);
		pCommandContext->DrawInstanced(3, 1, 0, 0);
		pCommandContext->TransitionBarrier(pRenderDevice->GetTexture(AssetTextures[BRDFLUT]), Resource::State::PixelShaderResource | Resource::State::NonPixelShaderResource);
	}

	pCommandContext->SetDescriptorHeaps(&m_CBSRUADescriptorHeap, nullptr);

	// Generate skybox first
	AssetTextures[SkyboxEquirectangularMap] = LoadFromFile(Scene.Skybox.Path, false, true);
	auto& stagingTexture = m_UnstagedTextures[AssetTextures[SkyboxEquirectangularMap]];
	StageTexture(AssetTextures[SkyboxEquirectangularMap], stagingTexture, pCommandContext);

	// Generate cubemap for equirectangular map
	Texture* pEquirectangularMap = pRenderDevice->GetTexture(AssetTextures[SkyboxEquirectangularMap]);
	AssetTextures[SkyboxCubemap] = pRenderDevice->CreateTexture(Resource::Type::TextureCube, [&](TextureProxy& proxy)
	{
		proxy.SetFormat(pEquirectangularMap->GetFormat());
		proxy.SetWidth(1024);
		proxy.SetHeight(1024);
		proxy.SetMipLevels(0);
		proxy.BindFlags = Resource::BindFlags::UnorderedAccess;
		proxy.InitialState = Resource::State::Common;
	});

	EquirectangularToCubemap(AssetTextures[SkyboxEquirectangularMap], AssetTextures[SkyboxCubemap], pCommandContext);

	// Create temporary srv for the cubemap and generate convolutions
	DescriptorAllocation tempSkyboxSRV = m_CBSRUADescriptorHeap.AllocateSRDescriptors(1).value();
	pRenderDevice->CreateSRV(AssetTextures[SkyboxCubemap], tempSkyboxSRV[0]);
	m_TemporaryDescriptorAllocations.push_back(tempSkyboxSRV);
	pCommandContext->TransitionBarrier(pRenderDevice->GetTexture(AssetTextures[SkyboxCubemap]), Resource::State::PixelShaderResource | Resource::State::NonPixelShaderResource);

	// Generate cubemap convolutions
	for (int i = 0; i < CubemapConvolution::CubemapConvolution_Count; ++i)
	{
		const DXGI_FORMAT format = i == CubemapConvolution::Irradiance ? RendererFormats::IrradianceFormat : RendererFormats::PrefilterFormat;
		const UINT64 resolution = i == CubemapConvolution::Irradiance ? Resolutions::Irradiance : Resolutions::Prefilter;
		const UINT16 numMips = static_cast<UINT16>(floor(log2(resolution))) + 1;

		// Create cubemap texture
		RenderResourceHandle cubemap = pRenderDevice->CreateTexture(Resource::Type::TextureCube, [&](TextureProxy& proxy)
		{
			proxy.SetFormat(format);
			proxy.SetWidth(resolution);
			proxy.SetHeight(resolution);
			proxy.SetMipLevels(numMips);
			proxy.BindFlags = Resource::BindFlags::RenderTarget;
			proxy.InitialState = Resource::State::RenderTarget;
		});

		UINT numDescriptorsToAllocate = numMips * 6;
		DescriptorAllocation tempCubemapRTVs = pRenderDevice->GetDescriptorAllocator().AllocateRenderTargetDescriptors(numDescriptorsToAllocate);
		m_TemporaryDescriptorAllocations.push_back(tempCubemapRTVs);

		// Generate cube map
		pCommandContext->SetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
		pCommandContext->SetPipelineState(pRenderDevice->GetGraphicsPSO(i == CubemapConvolution::Irradiance ? GraphicsPSOs::ConvolutionIrradiace : GraphicsPSOs::ConvolutionPrefilter));
		pCommandContext->SetGraphicsRootSignature(pRenderDevice->GetRootSignature(i == CubemapConvolution::Irradiance ? RootSignatures::ConvolutionIrradiace : RootSignatures::ConvolutionPrefilter));

		pCommandContext->SetGraphicsRootDescriptorTable(RootParameters::CubemapConvolution::CubemapSRV, tempSkyboxSRV[0].GPUHandle);

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

			pCommandContext->SetViewports(1, &vp);
			pCommandContext->SetScissorRects(1, &sr);
			for (UINT face = 0; face < 6; ++face)
			{
				UINT renderTargetDescriptorOffset = UINT(INT64(mip) * INT64(6) + INT64(face));
				pRenderDevice->CreateRTV(cubemap, tempCubemapRTVs[renderTargetDescriptorOffset], face, mip, 1);
				pCommandContext->SetRenderTargets(1, tempCubemapRTVs[renderTargetDescriptorOffset], TRUE, Descriptor());

				Buffer* pCubemapCameras = pRenderDevice->GetBuffer(m_CubemapCamerasUploadBufferHandle);
				pCommandContext->SetGraphicsRootConstantBufferView(RootParameters::CubemapConvolution::RenderPassCBuffer, pCubemapCameras->GetGpuVirtualAddressAt(face));
				switch (i)
				{
				case Irradiance:
				{
					ConvolutionIrradianceSetting icSetting;
					pCommandContext->SetGraphicsRoot32BitConstants(RootParameters::CubemapConvolution::Setting, sizeof(icSetting) / 4, &icSetting, 0);
				}
				break;
				case Prefilter:
				{
					ConvolutionPrefilterSetting pcSetting;
					pcSetting.Roughness = (float)mip / (float)(numMips - 1);
					pCommandContext->SetGraphicsRoot32BitConstants(RootParameters::CubemapConvolution::Setting, sizeof(pcSetting) / 4, &pcSetting, 0);
				}
				break;
				}
				pCommandContext->DrawIndexedInstanced(Scene.Skybox.Mesh.IndexCount, 1, Scene.Skybox.Mesh.StartIndexLocation, Scene.Skybox.Mesh.BaseVertexLocation, 0);
			}
		}

		// Set cubemap
		switch (i)
		{
		case Irradiance: AssetTextures[SkyboxIrradianceCubemap] = cubemap; break;
		case Prefilter: AssetTextures[SkyboxPrefilteredCubemap] = cubemap; break;
		}
	}

	for (auto& material : Scene.Materials)
	{
		LoadMaterial(material);
	}

	// Gpu copy
	for (auto& [handle, stagingTexture] : m_UnstagedTextures)
	{
		// Skybox is already staged, dont need to stage again
		if (handle == AssetTextures[SkyboxEquirectangularMap])
			continue;

		StageTexture(handle, stagingTexture, pCommandContext);
	}

	// Update material's gpu texture index
	for (auto& material : Scene.Materials)
	{
		for (unsigned int i = 0; i < NumTextureTypes; ++i)
		{
			auto textureIndexIter = TextureStorage.TextureIndices.find(material.Textures[i].Path.generic_string());
			if (textureIndexIter != TextureStorage.TextureIndices.end())
			{
				material.TextureIndices[i] = textureIndexIter->second;
			}
		}
		if (!material.Textures[Albedo].Path.empty())
		{
			auto textureHandleIter = TextureStorage.TextureHandles.find(material.Textures[Albedo].Path.generic_string());
			material.IsMasked = m_UnstagedTextures[textureHandleIter->second].isMasked;
		}
	}
}

void GpuTextureAllocator::Update(Scene& Scene)
{
	std::size_t i = 0;
	MaterialTextureIndices materialTextureIndices;
	MaterialTextureProperties materialTextureProperties;
	for (auto& material : Scene.Materials)
	{
		materialTextureIndices.AlbedoMapIndex = material.TextureIndices[TextureTypes::Albedo];
		materialTextureIndices.NormalMapIndex = material.TextureIndices[TextureTypes::Normal];
		materialTextureIndices.RoughnessMapIndex = material.TextureIndices[TextureTypes::Roughness];
		materialTextureIndices.MetallicMapIndex = material.TextureIndices[TextureTypes::Metallic];
		materialTextureIndices.EmissiveMapIndex = material.TextureIndices[TextureTypes::Emissive];
		materialTextureIndices.IsMasked = material.IsMasked;
		m_pMaterialTextureIndicesStructuredBuffer->Update<MaterialTextureIndices>(i, materialTextureIndices);

		materialTextureProperties.Albedo = material.Properties.Albedo;
		materialTextureProperties.Roughness = material.Properties.Roughness;
		materialTextureProperties.Metallic = material.Properties.Metallic;
		materialTextureProperties.Emissive = material.Properties.Emissive;
		m_pMaterialTexturePropertiesStructuredBuffer->Update<MaterialTextureProperties>(i, materialTextureProperties);
		i++;
	}
}

void GpuTextureAllocator::Bind(std::optional<UINT> MaterialTextureIndicesRootParameterIndex, std::optional<UINT> MaterialTexturePropertiesRootParameterIndex, CommandContext* pCommandContext) const
{
	if (MaterialTextureIndicesRootParameterIndex.has_value())
	{
		if (pCommandContext->GetType() == CommandContext::Type::Direct)
			pCommandContext->SetGraphicsRootShaderResourceView(MaterialTextureIndicesRootParameterIndex.value(), m_pMaterialTextureIndicesStructuredBuffer->GetGpuVirtualAddress());
		else if (pCommandContext->GetType() == CommandContext::Type::Compute)
			pCommandContext->SetComputeRootShaderResourceView(MaterialTextureIndicesRootParameterIndex.value(), m_pMaterialTextureIndicesStructuredBuffer->GetGpuVirtualAddress());
	}

	if (MaterialTexturePropertiesRootParameterIndex.has_value())
	{
		if (pCommandContext->GetType() == CommandContext::Type::Direct)
			pCommandContext->SetGraphicsRootShaderResourceView(MaterialTexturePropertiesRootParameterIndex.value(), m_pMaterialTexturePropertiesStructuredBuffer->GetGpuVirtualAddress());
		else if (pCommandContext->GetType() == CommandContext::Type::Compute)
			pCommandContext->SetComputeRootShaderResourceView(MaterialTexturePropertiesRootParameterIndex.value(), m_pMaterialTexturePropertiesStructuredBuffer->GetGpuVirtualAddress());
	}
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
		handle = pRenderDevice->CreateTexture(Resource::Type::Texture1D, [&](TextureProxy& proxy)
		{
			proxy.SetFormat(metadata.format);
			proxy.SetWidth(static_cast<UINT64>(metadata.width));
			proxy.SetDepthOrArraySize(static_cast<UINT16>(metadata.arraySize));
			proxy.SetMipLevels(mipLevels);
			proxy.BindFlags = Resource::BindFlags::UnorderedAccess;
			proxy.InitialState = Resource::State::CopyDest;
		});
	}
	break;

	case DirectX::TEX_DIMENSION::TEX_DIMENSION_TEXTURE2D:
	{
		handle = pRenderDevice->CreateTexture(Resource::Type::Texture2D, [&](TextureProxy& proxy)
		{
			proxy.SetFormat(metadata.format);
			proxy.SetWidth(static_cast<UINT64>(metadata.width));
			proxy.SetHeight(static_cast<UINT>(metadata.height));
			proxy.SetDepthOrArraySize(static_cast<UINT16>(metadata.arraySize));
			proxy.SetMipLevels(mipLevels);
			proxy.BindFlags = Resource::BindFlags::UnorderedAccess;
			proxy.InitialState = Resource::State::CopyDest;
		});
	}
	break;

	case DirectX::TEX_DIMENSION::TEX_DIMENSION_TEXTURE3D:
	{
		handle = pRenderDevice->CreateTexture(Resource::Type::Texture3D, [&](TextureProxy& proxy)
		{
			proxy.SetFormat(metadata.format);
			proxy.SetWidth(static_cast<UINT64>(metadata.width));
			proxy.SetHeight(static_cast<UINT>(metadata.height));
			proxy.SetDepthOrArraySize(static_cast<UINT16>(metadata.depth));
			proxy.SetMipLevels(mipLevels);
			proxy.BindFlags = Resource::BindFlags::UnorderedAccess;
			proxy.InitialState = Resource::State::CopyDest;
		});
	}
	break;

	default:
	{
		assert("Should not get here");
	}
	}

	Texture* pTexture = pRenderDevice->GetTexture(handle);
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

	auto pD3DDevice = pRenderDevice->GetDevice().GetD3DDevice();
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
	stagingTexture.texture = Texture(stagingResource);
	stagingTexture.numSubresources = NumSubresources;
	stagingTexture.placedSubresourceLayouts = std::move(placedSubresourceLayouts);
	stagingTexture.mipLevels = mipLevels;
	stagingTexture.index = m_NumTextures++;
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
			bool srgb = i == Albedo || i == Emissive;

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

void GpuTextureAllocator::StageTexture(RenderResourceHandle TextureHandle, StagingTexture& StagingTexture, CommandContext* pCommandContext)
{
	Texture* pTexture = pRenderDevice->GetTexture(TextureHandle);
	Texture* pStagingResourceTexture = &StagingTexture.texture;

	// Stage texture
	pCommandContext->TransitionBarrier(pTexture, Resource::State::CopyDest);
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
		pCommandContext->CopyTextureRegion(&destination, 0, 0, 0, &source, nullptr);
	}

	if (StagingTexture.generateMips)
	{
		GenerateMips(TextureHandle, pCommandContext);
	}

	pCommandContext->TransitionBarrier(pTexture, Resource::State::PixelShaderResource | Resource::State::NonPixelShaderResource);

	TextureStorage.AddTexture(StagingTexture.path, TextureHandle, StagingTexture.index);
	CORE_INFO("{} Loaded", StagingTexture.path);
}

void GpuTextureAllocator::GenerateMips(RenderResourceHandle TextureHandle, CommandContext* pCommandContext)
{
	Texture* pTexture = pRenderDevice->GetTexture(TextureHandle);
	if (IsUAVCompatable(pTexture->GetFormat()))
	{
		GenerateMipsUAV(TextureHandle, pCommandContext);
	}
	else if (IsSRGB(pTexture->GetFormat()))
	{
		GenerateMipsSRGB(TextureHandle, pCommandContext);
	}
}

void GpuTextureAllocator::GenerateMipsUAV(RenderResourceHandle TextureHandle, CommandContext* pCommandContext)
{
	// Credit: https://github.com/jpvanoosten/LearningDirectX12/blob/master/DX12Lib/src/CommandList.cpp
	DescriptorAllocation tempSRV = m_CBSRUADescriptorHeap.AllocateSRDescriptors(1).value();
	pRenderDevice->CreateSRV(TextureHandle, tempSRV[0]);
	m_TemporaryDescriptorAllocations.push_back(tempSRV);

	Texture* pTexture = pRenderDevice->GetTexture(TextureHandle);

	pCommandContext->SetPipelineState(pRenderDevice->GetComputePSO(ComputePSOs::GenerateMips));
	pCommandContext->SetComputeRootSignature(pRenderDevice->GetRootSignature(RootSignatures::GenerateMips));

	GenerateMipsData generateMipsCB;
	generateMipsCB.IsSRGB = DirectX::IsSRGB(pTexture->GetFormat());

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
		generateMipsCB.SrcDimension = (srcHeight & 1) << 1 | (srcWidth & 1);

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

		generateMipsCB.SrcMipLevel = srcMip;
		generateMipsCB.NumMipLevels = mipCount;
		generateMipsCB.TexelSize.x = 1.0f / (float)dstWidth;
		generateMipsCB.TexelSize.y = 1.0f / (float)dstHeight;

		pCommandContext->SetComputeRoot32BitConstants(RootParameters::GenerateMips::GenerateMipsCBuffer, sizeof(GenerateMipsData) / 4, &generateMipsCB, 0);

		pCommandContext->TransitionBarrier(pTexture, Resource::State::NonPixelShaderResource, srcMip);
		pCommandContext->FlushResourceBarriers();
		pCommandContext->SetComputeRootDescriptorTable(RootParameters::GenerateMips::SrcMip, tempSRV.StartDescriptor.GPUHandle);

		DescriptorAllocation tempUAVs = m_CBSRUADescriptorHeap.AllocateUADescriptors(4).value();
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

			pRenderDevice->GetDevice().GetD3DDevice()->CreateUnorderedAccessView(nullptr, nullptr, &uavDesc, tempUAVs[i].CPUHandle);
		}
		m_TemporaryDescriptorAllocations.push_back(tempUAVs);

		for (uint32_t mip = 0; mip < mipCount; ++mip)
		{
			pRenderDevice->CreateUAV(TextureHandle, tempUAVs[mip], {}, srcMip + mip + 1);

			pCommandContext->TransitionBarrier(pTexture, Resource::State::UnorderedAccess, srcMip + mip + 1);
		}
		pCommandContext->FlushResourceBarriers();
		pCommandContext->SetComputeRootDescriptorTable(RootParameters::GenerateMips::OutMips, tempUAVs.StartDescriptor.GPUHandle);

		UINT threadGroupCountX = Math::DivideByMultiple(dstWidth, 8);
		UINT threadGroupCountY = Math::DivideByMultiple(dstHeight, 8);
		pCommandContext->Dispatch(threadGroupCountX, threadGroupCountY, 1);

		pCommandContext->UAVBarrier(pTexture);
		pCommandContext->FlushResourceBarriers();

		srcMip += mipCount;
	}
}

void GpuTextureAllocator::GenerateMipsSRGB(RenderResourceHandle TextureHandle, CommandContext* pCommandContext)
{
	Texture* pTexture = pRenderDevice->GetTexture(TextureHandle);

	RenderResourceHandle textureCopyHandle = pRenderDevice->CreateTexture(pTexture->GetType(), [&](TextureProxy& proxy)
	{
		proxy.SetFormat(pTexture->GetFormat());
		proxy.SetWidth(pTexture->GetWidth());
		proxy.SetHeight(pTexture->GetHeight());
		proxy.SetDepthOrArraySize(pTexture->GetDepthOrArraySize());
		proxy.SetMipLevels(pTexture->GetMipLevels());
		proxy.BindFlags = Resource::BindFlags::UnorderedAccess;
		proxy.InitialState = Resource::State::CopyDest;
	});

	Texture* pDstTexture = pRenderDevice->GetTexture(textureCopyHandle);

	pCommandContext->CopyResource(pDstTexture, pTexture);

	GenerateMipsUAV(textureCopyHandle, pCommandContext);

	pCommandContext->CopyResource(pTexture, pDstTexture);

	m_TemporaryResources.push_back(textureCopyHandle);
}

void GpuTextureAllocator::EquirectangularToCubemap(RenderResourceHandle EquirectangularMap, RenderResourceHandle Cubemap, CommandContext* pCommandContext)
{
	Texture* pCubemap = pRenderDevice->GetTexture(Cubemap);
	if (IsUAVCompatable(pCubemap->GetFormat()))
	{
		EquirectangularToCubemapUAV(EquirectangularMap, Cubemap, pCommandContext);
	}
	else if (IsSRGB(pCubemap->GetFormat()))
	{
		EquirectangularToCubemapSRGB(EquirectangularMap, Cubemap, pCommandContext);
	}
}

void GpuTextureAllocator::EquirectangularToCubemapUAV(RenderResourceHandle EquirectangularMap, RenderResourceHandle Cubemap, CommandContext* pCommandContext)
{
	DescriptorAllocation tempSRV = m_CBSRUADescriptorHeap.AllocateSRDescriptors(1).value();
	pRenderDevice->CreateSRV(EquirectangularMap, tempSRV[0]);
	m_TemporaryDescriptorAllocations.push_back(tempSRV);

	Texture* pEquirectangularMap = pRenderDevice->GetTexture(EquirectangularMap);
	Texture* pCubemap = pRenderDevice->GetTexture(Cubemap);
	auto resourceDesc = pCubemap->GetD3DResource()->GetDesc();

	pCommandContext->SetPipelineState(pRenderDevice->GetComputePSO(ComputePSOs::EquirectangularToCubemap));
	pCommandContext->SetComputeRootSignature(pRenderDevice->GetRootSignature(RootSignatures::EquirectangularToCubemap));

	EquirectangularToCubemapData panoToCubemapCB;

	for (uint32_t mipSlice = 0; mipSlice < resourceDesc.MipLevels; )
	{
		// Maximum number of mips to generate per pass is 5.
		uint32_t numMips = std::min<uint32_t>(5, resourceDesc.MipLevels - mipSlice);

		panoToCubemapCB.FirstMip = mipSlice;
		panoToCubemapCB.CubemapSize = std::max<uint32_t>(static_cast<uint32_t>(resourceDesc.Width), resourceDesc.Height) >> mipSlice;
		panoToCubemapCB.NumMips = numMips;

		pCommandContext->SetComputeRoot32BitConstants(RootParameters::EquirectangularToCubemap::PanoToCubemapCBuffer, 3, &panoToCubemapCB, 0);

		pCommandContext->TransitionBarrier(pEquirectangularMap, Resource::State::NonPixelShaderResource);
		pCommandContext->SetComputeRootDescriptorTable(RootParameters::EquirectangularToCubemap::SrcMip, tempSRV[0].GPUHandle);

		DescriptorAllocation tempUAVs = m_CBSRUADescriptorHeap.AllocateUADescriptors(5).value();
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
			pRenderDevice->GetDevice().GetD3DDevice()->CreateUnorderedAccessView(nullptr, nullptr, &uavDesc, tempUAVs[i].CPUHandle);
		}
		m_TemporaryDescriptorAllocations.push_back(tempUAVs);

		for (uint32_t mip = 0; mip < numMips; ++mip)
		{
			pRenderDevice->CreateUAV(Cubemap, tempUAVs[mip], {}, mipSlice + mip);

			pCommandContext->TransitionBarrier(pCubemap, Resource::State::UnorderedAccess);
		}
		pCommandContext->SetComputeRootDescriptorTable(RootParameters::EquirectangularToCubemap::OutMips, tempUAVs.StartDescriptor.GPUHandle);

		UINT threadGroupCount = Math::DivideByMultiple(panoToCubemapCB.CubemapSize, 16);
		pCommandContext->Dispatch(threadGroupCount, threadGroupCount, 6);

		pCommandContext->UAVBarrier(pCubemap);

		mipSlice += numMips;
	}
}

void GpuTextureAllocator::EquirectangularToCubemapSRGB(RenderResourceHandle EquirectangularMap, RenderResourceHandle Cubemap, CommandContext* pCommandContext)
{
	Texture* pCubemap = pRenderDevice->GetTexture(Cubemap);

	RenderResourceHandle textureCopyHandle = pRenderDevice->CreateTexture(pCubemap->GetType(), [&](TextureProxy& proxy)
	{
		proxy.SetFormat(pCubemap->GetFormat());
		proxy.SetWidth(pCubemap->GetWidth());
		proxy.SetHeight(pCubemap->GetHeight());
		proxy.SetDepthOrArraySize(pCubemap->GetDepthOrArraySize());
		proxy.SetMipLevels(pCubemap->GetMipLevels());
		proxy.BindFlags = Resource::BindFlags::UnorderedAccess;
		proxy.InitialState = Resource::State::CopyDest;
	});

	Texture* pDstTexture = pRenderDevice->GetTexture(textureCopyHandle);

	pCommandContext->CopyResource(pDstTexture, pCubemap);

	EquirectangularToCubemapUAV(EquirectangularMap, textureCopyHandle, pCommandContext);

	pCommandContext->CopyResource(pCubemap, pDstTexture);

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