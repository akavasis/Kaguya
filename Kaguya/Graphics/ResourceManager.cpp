#include "pch.h"
#include "ResourceManager.h"

#include <ResourceUploadBatch.h>
#include <DDSTextureLoader.h>
#include <WICTextureLoader.h>

#include <assimp/Importer.hpp>
#include <assimp/scene.h>
#include <assimp/postprocess.h>
#include <assimp/pbrmaterial.h>

using namespace DirectX;
using Microsoft::WRL::ComPtr;

struct TextureDesc
{
	std::filesystem::path Path;
	bool sRGB;
	std::function<void()> Callback;
};

struct MeshDesc
{
	std::filesystem::path Path;
	bool KeepGeometryInRAM;
	std::function<void()> Callback;
};

struct PendingTexture2D
{
	PendingTexture2D() = default;
	PendingTexture2D(const PendingTexture2D&) = delete;
	PendingTexture2D& operator=(const PendingTexture2D&) = delete;
	PendingTexture2D(PendingTexture2D&&) = default;
	PendingTexture2D& operator=(PendingTexture2D&&) = default;

	TextureDesc Desc;
	ScratchImage Image;
};

struct PendingMesh
{
	PendingMesh() = default;
	PendingMesh(const PendingMesh&) = delete;
	PendingMesh& operator=(const PendingMesh&) = delete;
	PendingMesh(PendingMesh&&) = default;
	PendingMesh& operator=(PendingMesh&&) = default;

	MeshDesc Desc;
	std::vector<Vertex> Vertices;
	std::vector<unsigned int> Indices;
};

namespace
{
	static Assimp::Importer Importer;

	ConditionVariable ProducerCV, ConsumerCV;
	CriticalSection ProducerCS, ConsumerCS;

	ThreadSafeQueue<TextureDesc> TextureDescQueue;
	ThreadSafeQueue<MeshDesc> MeshDescQueue;
	ThreadSafeQueue<PendingTexture2D> PendingTextures;
	ThreadSafeQueue<PendingMesh> PendingMeshes;
}

ResourceManager::ResourceManager(RenderDevice* pRenderDevice)
	: pRenderDevice(pRenderDevice)
{
	CreateSystemTextures();

	ResourceThreads[0].reset(
		::CreateThread(NULL, 0, ResourceManager::ResourceProducerThreadProc, this, 0, nullptr));

	ResourceThreads[1].reset(
		::CreateThread(NULL, 0, ResourceManager::ResourceConsumerThreadProc, this, 0, nullptr));
}

ResourceManager::~ResourceManager()
{
	ExitResourceProcessThread = true;
	ProducerCV.WakeAll();
	ConsumerCV.WakeAll();

	::WaitForMultipleObjects(ARRAYSIZE(ResourceThreads), ResourceThreads[0].addressof(), TRUE, INFINITE);
}

Descriptor ResourceManager::GetTexture(const std::string& Name)
{
	ScopedReadLock SRL(TextureCacheLock);
	if (auto iter = TextureCache.find(Name);
		iter != TextureCache.end())
	{
		return iter->second.SRV;
	}

	return GetDefaultBlackTexture();
}

void ResourceManager::AsyncLoadTexture2D(const std::filesystem::path& Path, bool sRGB, std::function<void()> Callback /*= nullptr*/)
{
	if (!std::filesystem::exists(Path))
	{
		return;
	}

	ScopedReadLock SRL(TextureCacheLock);
	const auto Key = Path.string();
	if (TextureCache.find(Key) != TextureCache.end())
	{
		return;
	}

	TextureDesc Info =
	{
		.Path = Path,
		.sRGB = sRGB,
		.Callback = std::move(Callback)
	};
	TextureDescQueue.Enqueue(std::move(Info));

	ProducerCV.Wake();
}

void ResourceManager::AsyncLoadMesh(const std::filesystem::path& Path, bool KeepGeometryInRAM, std::function<void()> Callback /*= nullptr*/)
{
	if (!std::filesystem::exists(Path))
	{
		return;
	}

	ScopedReadLock SRL(MeshCacheLock);
	const auto Key = Path.string();
	if (MeshCache.find(Key) != MeshCache.end())
	{
		return;
	}

	MeshDesc Info =
	{
		.Path = Path,
		.KeepGeometryInRAM = KeepGeometryInRAM,
		.Callback = std::move(Callback)
	};
	MeshDescQueue.Enqueue(std::move(Info));

	ProducerCV.Wake();
}

void ResourceManager::CreateSystemTextures()
{
	const std::filesystem::path AssetPaths[AssetTextures::NumSystemTextures] =
	{
		Application::ExecutableFolderPath / "Assets/Textures/DefaultWhite.dds",
		Application::ExecutableFolderPath / "Assets/Textures/DefaultBlack.dds",
		Application::ExecutableFolderPath / "Assets/Textures/DefaultAlbedoMap.dds",
		Application::ExecutableFolderPath / "Assets/Textures/DefaultNormalMap.dds",
		Application::ExecutableFolderPath / "Assets/Textures/DefaultRoughnessMap.dds"
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

	auto ResourceAllocationInfo = pRenderDevice->Device->GetResourceAllocationInfo1(0, ARRAYSIZE(ResourceDescs), ResourceDescs, ResourceAllocationInfo1);
	if (ResourceAllocationInfo.Alignment != D3D12_SMALL_RESOURCE_PLACEMENT_ALIGNMENT)
	{
		// If the alignment requested is not granted, then let D3D tell us
		// the alignment that needs to be used for these resources.
		for (auto& ResourceDesc : ResourceDescs)
		{
			ResourceDesc.Alignment = 0;
		}
		ResourceAllocationInfo = pRenderDevice->Device->GetResourceAllocationInfo1(0, ARRAYSIZE(ResourceDescs), ResourceDescs, ResourceAllocationInfo1);
	}

	auto HeapDesc = CD3DX12_HEAP_DESC(ResourceAllocationInfo.SizeInBytes, D3D12_HEAP_TYPE_DEFAULT, 0, D3D12_HEAP_FLAG_DENY_BUFFERS | D3D12_HEAP_FLAG_DENY_RT_DS_TEXTURES);
	pRenderDevice->Device->CreateHeap(&HeapDesc, IID_PPV_ARGS(SystemTextureHeap.ReleaseAndGetAddressOf()));

	ResourceUploadBatch Uploader(pRenderDevice->Device);
	Uploader.Begin(D3D12_COMMAND_LIST_TYPE_COPY);

	for (size_t i = 0; i < AssetTextures::NumSystemTextures; ++i)
	{
		const auto& Path = AssetPaths[i];

		UINT64 HeapOffset = ResourceAllocationInfo1[i].Offset;
		pRenderDevice->Device->CreatePlacedResource(SystemTextureHeap.Get(), HeapOffset,
			&ResourceDescs[i], D3D12_RESOURCE_STATE_COMMON, nullptr,
			IID_PPV_ARGS(SystemTextures[i].ReleaseAndGetAddressOf()));

		SystemTextureSRVs[i] = pRenderDevice->AllocateShaderResourceView();
		pRenderDevice->CreateShaderResourceView(SystemTextures[i].Get(), SystemTextureSRVs[i]); // Create SRV

		// Upload
		const auto& Image = ScratchImages[i];
		std::vector<D3D12_SUBRESOURCE_DATA> subresources(Image.GetImageCount());
		const DirectX::Image* pImages = Image.GetImages();
		for (size_t i = 0; i < Image.GetImageCount(); ++i)
		{
			subresources[i].RowPitch = pImages[i].rowPitch;
			subresources[i].SlicePitch = pImages[i].slicePitch;
			subresources[i].pData = pImages[i].pixels;
		}

		Uploader.Upload(SystemTextures[i].Get(), 0, subresources.data(), subresources.size());
	}

	// Upload the resources to the GPU.
	auto finish = Uploader.End(this->pRenderDevice->CopyQueue);
	finish.wait();
}

bool ProcessTexture()
{
	bool Processed = false;

	TextureDesc Desc;
	while (TextureDescQueue.Dequeue(Desc, 0))
	{
		Processed = true;

		const auto& Path = Desc.Path;
		const auto Extension = Desc.Path.extension().string();

		ScratchImage Image;
		if (Extension == ".dds")
		{
			ThrowIfFailed(LoadFromDDSFile(Path.c_str(), DDS_FLAGS::DDS_FLAGS_FORCE_RGB, nullptr, Image));
		}
		else if (Extension == ".tga")
		{
			ScratchImage BaseImage;
			ThrowIfFailed(LoadFromTGAFile(Path.c_str(), nullptr, BaseImage));
			ThrowIfFailed(GenerateMipMaps(*BaseImage.GetImage(0, 0, 0), TEX_FILTER_DEFAULT, 0, Image, false));
		}
		else if (Extension == ".hdr")
		{
			ScratchImage BaseImage;
			ThrowIfFailed(LoadFromHDRFile(Path.c_str(), nullptr, BaseImage));
			ThrowIfFailed(GenerateMipMaps(*BaseImage.GetImage(0, 0, 0), TEX_FILTER_DEFAULT, 0, Image, false));
		}
		else
		{
			ScratchImage BaseImage;
			ThrowIfFailed(LoadFromWICFile(Desc.Path.c_str(), WIC_FLAGS::WIC_FLAGS_FORCE_RGB, nullptr, BaseImage));
			ThrowIfFailed(GenerateMipMaps(*BaseImage.GetImage(0, 0, 0), TEX_FILTER_DEFAULT, 0, Image, false));
		}

		PendingTexture2D PendingTexture;
		PendingTexture.Desc = std::move(Desc);
		PendingTexture.Image = std::move(Image);

		PendingTextures.Enqueue(std::move(PendingTexture));
	}

	return Processed;
}

bool ProcessMesh()
{
	bool Processed = false;

	MeshDesc Desc;
	while (MeshDescQueue.Dequeue(Desc, 0))
	{
		Processed = true;

		const auto& Path = Desc.Path;
		const auto Key = Path.string();

		const aiScene* paiScene = Importer.ReadFile(Key.data(),
			aiProcess_ConvertToLeftHanded |
			aiProcessPreset_TargetRealtime_MaxQuality |
			aiProcess_OptimizeMeshes |
			aiProcess_OptimizeGraph);

		if (paiScene == nullptr)
		{
			LOG_ERROR("Assimp::Importer error: {}", Importer.GetErrorString());
			continue;
		}

		if (paiScene->mNumMeshes != 1)
		{
			LOG_ERROR("Num Meshes is greater than 1!");
			continue;
		}

		const aiMesh* paiMesh = paiScene->mMeshes[0];

		// Parse vertex data
		std::vector<Vertex> vertices(paiMesh->mNumVertices);

		// Parse vertex data
		for (unsigned int vertexIndex = 0; vertexIndex < paiMesh->mNumVertices; ++vertexIndex)
		{
			Vertex v = {};
			// Position
			v.Position = DirectX::XMFLOAT3(paiMesh->mVertices[vertexIndex].x, paiMesh->mVertices[vertexIndex].y, paiMesh->mVertices[vertexIndex].z);

			// Texture coords
			if (paiMesh->HasTextureCoords(0))
			{
				v.Texture = DirectX::XMFLOAT2(paiMesh->mTextureCoords[0][vertexIndex].x, paiMesh->mTextureCoords[0][vertexIndex].y);
			}

			// Normal
			if (paiMesh->HasNormals())
			{
				v.Normal = DirectX::XMFLOAT3(paiMesh->mNormals[vertexIndex].x, paiMesh->mNormals[vertexIndex].y, paiMesh->mNormals[vertexIndex].z);
			}

			vertices[vertexIndex] = v;
		}

		// Parse index data
		std::vector<std::uint32_t> indices;
		indices.reserve(size_t(paiMesh->mNumFaces) * 3);
		for (unsigned int faceIndex = 0; faceIndex < paiMesh->mNumFaces; ++faceIndex)
		{
			const aiFace& aiFace = paiMesh->mFaces[faceIndex];
			assert(aiFace.mNumIndices == 3);

			indices.push_back(aiFace.mIndices[0]);
			indices.push_back(aiFace.mIndices[1]);
			indices.push_back(aiFace.mIndices[2]);
		}

		PendingMesh PendingMesh;
		PendingMesh.Desc = std::move(Desc);
		PendingMesh.Vertices = std::move(vertices);
		PendingMesh.Indices = std::move(indices);

		PendingMeshes.Enqueue(std::move(PendingMesh));

		LOG_INFO("{} Loaded", Key);
	}

	return Processed;
}

DWORD WINAPI ResourceManager::ResourceProducerThreadProc(_In_ PVOID pParameter)
{
	SetThreadDescription(GetCurrentThread(), __FUNCTIONW__);

	auto pResourceManager = reinterpret_cast<ResourceManager*>(pParameter);
	auto pRenderDevice = pResourceManager->pRenderDevice;
	ID3D12Device* pDevice = pRenderDevice->Device;

	while (true)
	{
		ScopedCriticalSection SCS(ProducerCS);
		ProducerCV.Wait(ProducerCS, INFINITE);

		if (pResourceManager->ExitResourceProcessThread)
		{
			break;
		}

		bool Notify = ProcessTexture();
		Notify |= ProcessMesh();

		if (Notify)
		{
			ConsumerCV.Wake();
		}
	}

	return EXIT_SUCCESS;
}

auto ConsumeTexture(RenderDevice* pRenderDevice, ResourceUploadBatch& Uploader)
{
	std::unordered_map<std::string, Texture2D> LocalTextureCache;

	PendingTexture2D PendingTexture2D;
	while (PendingTextures.Dequeue(PendingTexture2D, 0))
	{
		const auto Key = PendingTexture2D.Desc.Path.string();
		if (LocalTextureCache.find(Key) != LocalTextureCache.end())
		{
			continue;
		}

		const auto& Image = PendingTexture2D.Image;
		const auto& Metadata = Image.GetMetadata();
		DXGI_FORMAT Format = Metadata.format;
		if (PendingTexture2D.Desc.sRGB)
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
		auto Texture = pRenderDevice->CreateResource(&AllocDesc, &Desc, D3D12_RESOURCE_STATE_COMMON, nullptr);

		Uploader.Upload(Texture->pResource.Get(), 0, subresources.data(), subresources.size());

		Descriptor SRV = pRenderDevice->AllocateShaderResourceView();

		pRenderDevice->CreateShaderResourceView(Texture->pResource.Get(), SRV);

		Texture2D Texture2D;
		Texture2D.Resource = std::move(Texture);
		Texture2D.SRV = SRV;

		LocalTextureCache[Key] = std::move(Texture2D);

		if (PendingTexture2D.Desc.Callback)
		{
			PendingTexture2D.Desc.Callback();
		}
	}

	return LocalTextureCache;
}

auto ConsumeMesh(RenderDevice* pRenderDevice, ResourceUploadBatch& Uploader)
{
	std::unordered_map<std::string, Mesh> LocalMeshCache;

	PendingMesh PendingMesh;
	while (PendingMeshes.Dequeue(PendingMesh, 0))
	{
		const auto Key = PendingMesh.Desc.Path.string();
		if (LocalMeshCache.find(Key) != LocalMeshCache.end())
		{
			continue;
		}

		UINT64 VBSizeInBytes = PendingMesh.Vertices.size() * sizeof(Vertex);
		UINT64 IBSizeInBytes = PendingMesh.Indices.size() * sizeof(unsigned int);

		D3D12MA::ALLOCATION_DESC AllocDesc = {};
		AllocDesc.HeapType = D3D12_HEAP_TYPE_DEFAULT;

		auto VB = pRenderDevice->CreateBuffer(&AllocDesc, VBSizeInBytes);
		auto IB = pRenderDevice->CreateBuffer(&AllocDesc, IBSizeInBytes);

		// Upload vertex data
		D3D12_SUBRESOURCE_DATA Subresource = {};
		Subresource.pData = PendingMesh.Vertices.data();
		Subresource.RowPitch = VBSizeInBytes;
		Subresource.SlicePitch = VBSizeInBytes;

		Uploader.Upload(VB->pResource.Get(), 0, &Subresource, 1);

		// Upload index data
		Subresource = {};
		Subresource.pData = PendingMesh.Indices.data();
		Subresource.RowPitch = IBSizeInBytes;
		Subresource.SlicePitch = IBSizeInBytes;

		Uploader.Upload(IB->pResource.Get(), 0, &Subresource, 1);

		Mesh Mesh(Key);

		// Parse aabb data for mesh
		DirectX::BoundingBox::CreateFromPoints(Mesh.BoundingBox, PendingMesh.Vertices.size(), &PendingMesh.Vertices[0].Position, sizeof(Vertex));

		// Parse mesh indices
		Mesh.VertexCount = PendingMesh.Vertices.size();
		Mesh.IndexCount = PendingMesh.Indices.size();

		if (PendingMesh.Desc.KeepGeometryInRAM)
		{
			Mesh.Vertices = std::move(PendingMesh.Vertices);
			Mesh.Indices = std::move(PendingMesh.Indices);
		}

		Mesh.VertexResource = std::move(VB);
		Mesh.IndexResource = std::move(IB);

		D3D12_RAYTRACING_GEOMETRY_DESC Desc = {};
		Desc.Type = D3D12_RAYTRACING_GEOMETRY_TYPE_TRIANGLES;
		Desc.Flags = D3D12_RAYTRACING_GEOMETRY_FLAG_OPAQUE;
		Desc.Triangles.Transform3x4 = NULL;
		Desc.Triangles.IndexFormat = DXGI_FORMAT_R32_UINT;
		Desc.Triangles.VertexFormat = DXGI_FORMAT_R32G32B32_FLOAT; // Position attribute of the vertex
		Desc.Triangles.IndexCount = Mesh.IndexCount;
		Desc.Triangles.VertexCount = Mesh.VertexCount;
		Desc.Triangles.IndexBuffer = Mesh.IndexResource->pResource->GetGPUVirtualAddress();
		Desc.Triangles.VertexBuffer.StartAddress = Mesh.VertexResource->pResource->GetGPUVirtualAddress();
		Desc.Triangles.VertexBuffer.StrideInBytes = sizeof(Vertex);

		Mesh.BLAS.AddGeometry(Desc);

		LocalMeshCache[Key] = std::move(Mesh);

		if (PendingMesh.Desc.Callback)
		{
			PendingMesh.Desc.Callback();
		}
	}

	return LocalMeshCache;
}

DWORD WINAPI ResourceManager::ResourceConsumerThreadProc(_In_ PVOID pParameter)
{
	SetThreadDescription(GetCurrentThread(), __FUNCTIONW__);

	auto pResourceManager = reinterpret_cast<ResourceManager*>(pParameter);
	auto pRenderDevice = pResourceManager->pRenderDevice;
	ID3D12Device5* pDevice = pRenderDevice->Device;

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
		ScopedCriticalSection SCS(ConsumerCS);
		ConsumerCV.Wait(ConsumerCS, INFINITE);

		if (pResourceManager->ExitResourceProcessThread)
		{
			break;
		}

		Uploader.Begin(D3D12_COMMAND_LIST_TYPE_COPY);

		auto LocalTextureCache = ConsumeTexture(pRenderDevice, Uploader);
		auto LocalMeshCache = ConsumeMesh(pRenderDevice, Uploader);

		// Upload the resources to the GPU.
		auto finish = Uploader.End(pRenderDevice->CopyQueue);
		finish.wait();

		std::vector<std::shared_ptr<Resource>> TrackedScratchBuffers;
		TrackedScratchBuffers.reserve(LocalMeshCache.size());
		mCmdAlloc->Reset();
		mCmdList->Reset(mCmdAlloc.Get(), nullptr);
		for (auto& iter : LocalMeshCache)
		{
			auto& Mesh = iter.second;
			auto& BLAS = iter.second.BLAS;

			UINT64 ScratchSIB, ResultSIB;
			BLAS.ComputeMemoryRequirements(pDevice, &ScratchSIB, &ResultSIB);

			// ASB seems to require a committed resource otherwise PIX gives you invalid
			// parameter
			D3D12MA::ALLOCATION_DESC AllocDesc = {};
			AllocDesc.Flags = D3D12MA::ALLOCATION_FLAG_COMMITTED;
			AllocDesc.HeapType = D3D12_HEAP_TYPE_DEFAULT;
			auto Scratch = pRenderDevice->CreateBuffer(&AllocDesc, ScratchSIB, D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS, 0, D3D12_RESOURCE_STATE_UNORDERED_ACCESS);
			auto Result = pRenderDevice->CreateBuffer(&AllocDesc, ResultSIB, D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS, 0, D3D12_RESOURCE_STATE_RAYTRACING_ACCELERATION_STRUCTURE);

			BLAS.Generate(mCmdList.Get(), Scratch->pResource.Get(), Result->pResource.Get());

			Mesh.AccelerationStructure = std::move(Result);
			TrackedScratchBuffers.push_back(std::move(Scratch));
		}
		mCmdList->Close();
		mCmdQueue->ExecuteCommandLists(1, CommandListCast(mCmdList.GetAddressOf()));
		auto FenceToWait = ++FenceValue;
		mCmdQueue->Signal(mFence.Get(), FenceToWait);
		mFence->SetEventOnCompletion(FenceToWait, Event.get());
		Event.wait();

		ScopedWriteLock SWL0(pResourceManager->TextureCacheLock);
		pResourceManager->TextureCache.insert(std::make_move_iterator(LocalTextureCache.begin()), std::make_move_iterator(LocalTextureCache.end()));

		ScopedWriteLock SWL1(pResourceManager->MeshCacheLock);
		pResourceManager->MeshCache.insert(std::make_move_iterator(LocalMeshCache.begin()), std::make_move_iterator(LocalMeshCache.end()));
	}

	return EXIT_SUCCESS;
}