#include "pch.h"
#include "AssetManager.h"

#include <ResourceUploadBatch.h>

using namespace DirectX;
using Microsoft::WRL::ComPtr;

static AssetManager* pAssetManager = nullptr;

void AssetManager::Initialize()
{
	if (!pAssetManager)
	{
		pAssetManager = new AssetManager();
	}
}

void AssetManager::Shutdown()
{
	if (pAssetManager)
	{
		delete pAssetManager;
	}
}

AssetManager& AssetManager::Instance()
{
	assert(pAssetManager);
	return *pAssetManager;
}

AssetManager::AssetManager()
{
	CreateSystemTextures();

	Thread.reset(::CreateThread(nullptr, 0, &ResourceUploadThreadProc, nullptr, 0, nullptr));
}

AssetManager::~AssetManager()
{
	ShutdownThread = true;
	UploadConditionVariable.WakeAll();

	::WaitForSingleObject(Thread.get(), INFINITE);
}

void AssetManager::AsyncLoadImage(const std::filesystem::path& Path, bool sRGB)
{
	if (!std::filesystem::exists(Path))
	{
		return;
	}

	entt::id_type hs = entt::hashed_string(Path.string().data());
	if (ImageCache.Exist(hs))
	{
		LOG_INFO("{} Exists", Path.string());
		return;
	}

	Asset::ImageMetadata metadata =
	{
		.Path = Path,
		.sRGB = sRGB
	};
	AsyncImageLoader.RequestAsyncLoad(1, &metadata,
		[&](auto pImage)
	{
		ScopedCriticalSection SCS(UploadCriticalSection);

		ImageUploadQueue.Enqueue(std::move(pImage));

		UploadConditionVariable.Wake();
	});
}

void AssetManager::AsyncLoadMesh(const std::filesystem::path& Path, bool KeepGeometryInRAM)
{
	if (!std::filesystem::exists(Path))
	{
		return;
	}

	entt::id_type hs = entt::hashed_string(Path.string().data());
	if (MeshCache.Exist(hs))
	{
		LOG_INFO("{} Exists", Path.string());
		return;
	}

	Asset::MeshMetadata metadata =
	{
		.Path = Path,
		.KeepGeometryInRAM = KeepGeometryInRAM,
	};
	AsyncMeshLoader.RequestAsyncLoad(1, &metadata,
		[&](auto pMesh)
	{
		ScopedCriticalSection SCS(UploadCriticalSection);

		MeshUploadQueue.Enqueue(std::move(pMesh));

		UploadConditionVariable.Wake();
	});
}

void AssetManager::CreateSystemTextures()
{
	auto& RenderDevice = RenderDevice::Instance();

	const std::filesystem::path assetPaths[AssetTextures::NumSystemTextures] =
	{
		Application::ExecutableFolderPath / "Assets/Textures/DefaultWhite.dds",
		Application::ExecutableFolderPath / "Assets/Textures/DefaultBlack.dds",
		Application::ExecutableFolderPath / "Assets/Textures/DefaultAlbedoMap.dds",
		Application::ExecutableFolderPath / "Assets/Textures/DefaultNormalMap.dds",
		Application::ExecutableFolderPath / "Assets/Textures/DefaultRoughnessMap.dds"
	};

	D3D12_RESOURCE_DESC resourceDescs[AssetTextures::NumSystemTextures] = {};
	D3D12_RESOURCE_ALLOCATION_INFO1 resourceAllocationInfo1[AssetTextures::NumSystemTextures] = {};
	DirectX::TexMetadata texMetadatas[AssetTextures::NumSystemTextures] = {};
	DirectX::ScratchImage scratchImages[AssetTextures::NumSystemTextures] = {};

	for (size_t i = 0; i < AssetTextures::NumSystemTextures; ++i)
	{
		DirectX::TexMetadata texMetadata;
		DirectX::ScratchImage scratchImage;
		ThrowIfFailed(DirectX::LoadFromDDSFile(assetPaths[i].c_str(), DirectX::DDS_FLAGS::DDS_FLAGS_FORCE_RGB, &texMetadata, scratchImage));

		resourceDescs[i] = CD3DX12_RESOURCE_DESC::Tex2D(texMetadata.format,
			texMetadata.width,
			texMetadata.height,
			texMetadata.arraySize,
			texMetadata.mipLevels);

		// Since we are using small resources we can take advantage of 4KB
		// resource alignments. As long as the most detailed mip can fit in an
		// allocation less than 64KB, 4KB alignments can be used.
		//
		// When dealing with MSAA textures the rules are similar, but the minimum
		// alignment is 64KB for a texture whose most detailed mip can fit in an
		// allocation less than 4MB.
		resourceDescs[i].Alignment = D3D12_SMALL_RESOURCE_PLACEMENT_ALIGNMENT;
		texMetadatas[i] = texMetadata;
		scratchImages[i] = std::move(scratchImage);
	}

	auto resourceAllocationInfo = RenderDevice.Device->GetResourceAllocationInfo1(0, ARRAYSIZE(resourceDescs), resourceDescs, resourceAllocationInfo1);
	if (resourceAllocationInfo.Alignment != D3D12_SMALL_RESOURCE_PLACEMENT_ALIGNMENT)
	{
		// If the alignment requested is not granted, then let D3D tell us
		// the alignment that needs to be used for these resources.
		for (auto& resourceDesc : resourceDescs)
		{
			resourceDesc.Alignment = 0;
		}
		resourceAllocationInfo = RenderDevice.Device->GetResourceAllocationInfo1(0, ARRAYSIZE(resourceDescs), resourceDescs, resourceAllocationInfo1);
	}

	auto heapDesc = CD3DX12_HEAP_DESC(resourceAllocationInfo.SizeInBytes, D3D12_HEAP_TYPE_DEFAULT, 0, D3D12_HEAP_FLAG_DENY_BUFFERS | D3D12_HEAP_FLAG_DENY_RT_DS_TEXTURES);
	RenderDevice.Device->CreateHeap(&heapDesc, IID_PPV_ARGS(SystemTextureHeap.ReleaseAndGetAddressOf()));

	ResourceUploadBatch uploader(RenderDevice.Device);
	uploader.Begin(D3D12_COMMAND_LIST_TYPE_COPY);

	for (size_t i = 0; i < AssetTextures::NumSystemTextures; ++i)
	{
		UINT64 heapOffset = resourceAllocationInfo1[i].Offset;
		RenderDevice.Device->CreatePlacedResource(SystemTextureHeap.Get(), heapOffset,
			&resourceDescs[i], D3D12_RESOURCE_STATE_COMMON, nullptr,
			IID_PPV_ARGS(SystemTextures[i].ReleaseAndGetAddressOf()));

		SystemTextureSRVs[i] = RenderDevice.AllocateShaderResourceView();
		RenderDevice.CreateShaderResourceView(SystemTextures[i].Get(), SystemTextureSRVs[i]); // Create SRV

		// Upload
		const auto& scratchImage = scratchImages[i];
		std::vector<D3D12_SUBRESOURCE_DATA> subresources(scratchImage.GetImageCount());
		const DirectX::Image* pImages = scratchImage.GetImages();
		for (size_t i = 0; i < scratchImage.GetImageCount(); ++i)
		{
			subresources[i].RowPitch = pImages[i].rowPitch;
			subresources[i].SlicePitch = pImages[i].slicePitch;
			subresources[i].pData = pImages[i].pixels;
		}

		uploader.Upload(SystemTextures[i].Get(), 0, subresources.data(), subresources.size());
	}

	// Upload the resources to the GPU.
	auto finish = uploader.End(RenderDevice.CopyQueue);
	finish.wait();
}

DWORD WINAPI AssetManager::ResourceUploadThreadProc(_In_ PVOID pParameter)
{
	auto& RenderDevice = RenderDevice::Instance();
	auto& AssetManager = AssetManager::Instance();

	ID3D12Device5* pDevice = RenderDevice.Device;

	ResourceUploadBatch Uploader(pDevice);

	ComPtr<ID3D12CommandQueue> mCmdQueue;
	ComPtr<ID3D12CommandAllocator> mCmdAlloc;
	ComPtr<ID3D12GraphicsCommandList6> mCmdList;
	UINT64 FenceValue = 0;
	Microsoft::WRL::ComPtr<ID3D12Fence> mFence;
	wil::unique_event Event;

	auto Type = D3D12_COMMAND_LIST_TYPE_COMPUTE;
	D3D12_COMMAND_QUEUE_DESC Desc = {};
	Desc.Type = Type;
	Desc.Priority = D3D12_COMMAND_QUEUE_PRIORITY_NORMAL;
	Desc.Flags = D3D12_COMMAND_QUEUE_FLAG_NONE;
	Desc.NodeMask = 0;
	ThrowIfFailed(pDevice->CreateCommandQueue(&Desc, IID_PPV_ARGS(mCmdQueue.ReleaseAndGetAddressOf())));
	ThrowIfFailed(pDevice->CreateCommandAllocator(Type, IID_PPV_ARGS(mCmdAlloc.ReleaseAndGetAddressOf())));
	ThrowIfFailed(pDevice->CreateCommandList(1, Type, mCmdAlloc.Get(), nullptr, IID_PPV_ARGS(mCmdList.ReleaseAndGetAddressOf())));
	mCmdList->Close();
	ThrowIfFailed(pDevice->CreateFence(0, D3D12_FENCE_FLAG_NONE, IID_PPV_ARGS(mFence.ReleaseAndGetAddressOf())));
	Event.create();

	while (true)
	{
		ScopedCriticalSection SCS(AssetManager.UploadCriticalSection);
		AssetManager.UploadConditionVariable.Wait(AssetManager.UploadCriticalSection, INFINITE);

		if (AssetManager.ShutdownThread)
		{
			break;
		}

		Uploader.Begin(D3D12_COMMAND_LIST_TYPE_COPY);
		mCmdAlloc->Reset();
		mCmdList->Reset(mCmdAlloc.Get(), nullptr);

		std::vector<std::shared_ptr<Resource>> TrackedScratchBuffers;

		std::vector<std::shared_ptr<Asset::Image>> Images;
		std::vector<std::shared_ptr<Asset::Mesh>> Meshes;

		// Process Image
		{
			std::shared_ptr<Asset::Image> pImage;
			while (AssetManager.ImageUploadQueue.Dequeue(pImage, 0))
			{
				const auto& Image = pImage->Image;
				const auto& Metadata = Image.GetMetadata();
				DXGI_FORMAT Format = Metadata.format;
				if (pImage->Metadata.sRGB)
					Format = DirectX::MakeSRGB(Format);

				D3D12_RESOURCE_DESC Desc = {};
				switch (Metadata.dimension)
				{
				case TEX_DIMENSION::TEX_DIMENSION_TEXTURE1D:
					Desc = CD3DX12_RESOURCE_DESC::Tex1D(Format,
						static_cast<UINT64>(Metadata.width),
						static_cast<UINT16>(Metadata.arraySize));
					break;

				case TEX_DIMENSION::TEX_DIMENSION_TEXTURE2D:
					Desc = CD3DX12_RESOURCE_DESC::Tex2D(Format,
						static_cast<UINT64>(Metadata.width),
						static_cast<UINT>(Metadata.height),
						static_cast<UINT16>(Metadata.arraySize));
					break;

				case TEX_DIMENSION::TEX_DIMENSION_TEXTURE3D:
					Desc = CD3DX12_RESOURCE_DESC::Tex3D(Format,
						static_cast<UINT64>(Metadata.width),
						static_cast<UINT>(Metadata.height),
						static_cast<UINT16>(Metadata.depth));
					break;
				}

				std::vector<D3D12_SUBRESOURCE_DATA> subresources(Image.GetImageCount());
				const auto pImages = Image.GetImages();
				for (size_t i = 0; i < Image.GetImageCount(); ++i)
				{
					subresources[i].RowPitch = pImages[i].rowPitch;
					subresources[i].SlicePitch = pImages[i].slicePitch;
					subresources[i].pData = pImages[i].pixels;
				}

				D3D12MA::ALLOCATION_DESC AllocDesc = {};
				AllocDesc.HeapType = D3D12_HEAP_TYPE_DEFAULT;
				auto Resource = RenderDevice.CreateResource(&AllocDesc, &Desc, D3D12_RESOURCE_STATE_COMMON, nullptr);

				Uploader.Upload(Resource->pResource.Get(), 0, subresources.data(), subresources.size());

				Descriptor SRV = RenderDevice.AllocateShaderResourceView();

				RenderDevice.CreateShaderResourceView(Resource->pResource.Get(), SRV);

				pImage->Resource = std::move(Resource);
				pImage->SRV = std::move(SRV);

				Images.push_back(pImage);
			}
		}

		// Process Mesh
		{
			std::shared_ptr<Asset::Mesh> pMesh;
			while (AssetManager.MeshUploadQueue.Dequeue(pMesh, 0))
			{
				UINT64 VBSizeInBytes = pMesh->Vertices.size() * sizeof(Vertex);
				UINT64 IBSizeInBytes = pMesh->Indices.size() * sizeof(unsigned int);

				D3D12MA::ALLOCATION_DESC AllocDesc = {};
				AllocDesc.HeapType = D3D12_HEAP_TYPE_DEFAULT;

				auto VB = RenderDevice.CreateBuffer(&AllocDesc, VBSizeInBytes);
				auto IB = RenderDevice.CreateBuffer(&AllocDesc, IBSizeInBytes);

				// Upload vertex data
				D3D12_SUBRESOURCE_DATA Subresource = {};
				Subresource.pData = pMesh->Vertices.data();
				Subresource.RowPitch = VBSizeInBytes;
				Subresource.SlicePitch = VBSizeInBytes;

				Uploader.Upload(VB->pResource.Get(), 0, &Subresource, 1);

				// Upload index data
				Subresource = {};
				Subresource.pData = pMesh->Indices.data();
				Subresource.RowPitch = IBSizeInBytes;
				Subresource.SlicePitch = IBSizeInBytes;

				Uploader.Upload(IB->pResource.Get(), 0, &Subresource, 1);

				pMesh->VertexResource = std::move(VB);
				pMesh->IndexResource = std::move(IB);

				for (const auto& Submesh : pMesh->Submeshes)
				{
					D3D12_RAYTRACING_GEOMETRY_DESC Desc = {};
					Desc.Type = D3D12_RAYTRACING_GEOMETRY_TYPE_TRIANGLES;
					Desc.Flags = D3D12_RAYTRACING_GEOMETRY_FLAG_OPAQUE;
					Desc.Triangles.Transform3x4 = NULL;
					Desc.Triangles.IndexFormat = DXGI_FORMAT_R32_UINT;
					Desc.Triangles.VertexFormat = DXGI_FORMAT_R32G32B32_FLOAT; // Position attribute of the vertex
					Desc.Triangles.IndexCount = Submesh.IndexCount;
					Desc.Triangles.VertexCount = Submesh.VertexCount;
					Desc.Triangles.IndexBuffer = pMesh->IndexResource->pResource->GetGPUVirtualAddress() + Submesh.StartIndexLocation * sizeof(unsigned int);
					Desc.Triangles.VertexBuffer.StartAddress = pMesh->VertexResource->pResource->GetGPUVirtualAddress() + Submesh.BaseVertexLocation * sizeof(Vertex);
					Desc.Triangles.VertexBuffer.StrideInBytes = sizeof(Vertex);

					pMesh->BLAS.AddGeometry(Desc);
				}

				UINT64 ScratchSIB, ResultSIB;
				pMesh->BLAS.ComputeMemoryRequirements(pDevice, &ScratchSIB, &ResultSIB);

				// ASB seems to require a committed resource otherwise PIX gives you invalid
				// parameter
				AllocDesc = {};
				AllocDesc.Flags = D3D12MA::ALLOCATION_FLAG_COMMITTED;
				AllocDesc.HeapType = D3D12_HEAP_TYPE_DEFAULT;
				auto Scratch = RenderDevice.CreateBuffer(&AllocDesc, ScratchSIB, D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS, 0, D3D12_RESOURCE_STATE_UNORDERED_ACCESS);
				auto Result = RenderDevice.CreateBuffer(&AllocDesc, ResultSIB, D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS, 0, D3D12_RESOURCE_STATE_RAYTRACING_ACCELERATION_STRUCTURE);

				pMesh->BLAS.Generate(mCmdList.Get(), Scratch->pResource.Get(), Result->pResource.Get());

				pMesh->AccelerationStructure = std::move(Result);
				TrackedScratchBuffers.push_back(std::move(Scratch));

				Meshes.push_back(pMesh);
			}
		}

		auto Future = Uploader.End(RenderDevice.CopyQueue);
		Future.wait();

		mCmdList->Close();
		mCmdQueue->ExecuteCommandLists(1, CommandListCast(mCmdList.GetAddressOf()));
		auto FenceToWait = ++FenceValue;
		mCmdQueue->Signal(mFence.Get(), FenceToWait);
		mFence->SetEventOnCompletion(FenceToWait, Event.get());
		Event.wait();

		for (auto& Image : Images)
		{
			entt::id_type hs = entt::hashed_string(Image->Metadata.Path.string().data());

			AssetManager.ImageCache.Create(hs);
			auto Asset = AssetManager.ImageCache.Load(hs);
			*Asset = std::move(*Image);
		}

		for (auto& Mesh : Meshes)
		{
			entt::id_type hs = entt::hashed_string(Mesh->Metadata.Path.string().data());

			AssetManager.MeshCache.Create(hs);
			auto Asset = AssetManager.MeshCache.Load(hs);
			*Asset = std::move(*Mesh);
		}
	}

	return EXIT_SUCCESS;
}