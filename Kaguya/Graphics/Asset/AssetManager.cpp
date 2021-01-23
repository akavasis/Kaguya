#include "pch.h"
#include "AssetManager.h"

#include <assimp/Importer.hpp>
#include <assimp/scene.h>
#include <assimp/postprocess.h>
#include <assimp/pbrmaterial.h>

using namespace DirectX;

struct ImageDesc
{
	std::filesystem::path Path;
	bool sRGB;
	ImageCallback Callback;
};

struct MeshDesc
{
	std::filesystem::path Path;
	bool KeepGeometryInRAM;
	MeshCallback Callback;
};

ThreadSafeQueue<ImageDesc> ImageDescQueue;
ThreadSafeQueue<MeshDesc> MeshDescQueue;

AssetManager::AssetManager()
{
	Thread.reset(::CreateThread(nullptr, 0, &AssetStreamThreadProc, this, 0, 0));
}

AssetManager::~AssetManager()
{
	Shutdown = true;
	AssetStreamConditionVariable.WakeAll();

	::WaitForSingleObject(Thread.get(), INFINITE);
}

std::string AssetManager::GetName(const std::filesystem::path& Path)
{
	return Path.filename().string();
}

void AssetManager::AsyncLoadImage(const std::filesystem::path& Path, bool sRGB, ImageCallback Callback)
{
	ImageDesc Desc =
	{
		.Path = Path,
		.sRGB = sRGB,
		.Callback = std::move(Callback)
	};

	ScopedCriticalSection SCS(AssetStreamCriticalSection);
	ImageDescQueue.Enqueue(std::move(Desc));
	AssetStreamConditionVariable.Wake();
}

void AssetManager::AsyncLoadMesh(const std::filesystem::path& Path, bool KeepGeometryInRAM, MeshCallback Callback)
{
	MeshDesc Desc =
	{
		.Path = Path,
		.KeepGeometryInRAM = KeepGeometryInRAM,
		.Callback = std::move(Callback)
	};

	ScopedCriticalSection SCS(AssetStreamCriticalSection);
	MeshDescQueue.Enqueue(std::move(Desc));
	AssetStreamConditionVariable.Wake();
}

DWORD WINAPI AssetManager::AssetStreamThreadProc(_In_ PVOID pParameter)
{
	Assimp::Importer Importer;

	auto pAssetManager = static_cast<AssetManager*>(pParameter);

	while (true)
	{
		ScopedCriticalSection SCS(pAssetManager->AssetStreamCriticalSection);
		pAssetManager->AssetStreamConditionVariable.Wait(pAssetManager->AssetStreamCriticalSection, INFINITE);

		if (pAssetManager->Shutdown)
		{
			break;
		}

		ImageDesc ImageDesc;
		while (ImageDescQueue.Dequeue(ImageDesc, 0))
		{
			const auto& Path = ImageDesc.Path;
			const auto Extension = Path.extension().string();

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
				ThrowIfFailed(LoadFromWICFile(Path.c_str(), WIC_FLAGS::WIC_FLAGS_FORCE_RGB, nullptr, BaseImage));
				ThrowIfFailed(GenerateMipMaps(*BaseImage.GetImage(0, 0, 0), TEX_FILTER_DEFAULT, 0, Image, false));
			}

			auto pImage = std::make_shared<Asset::Image>();

			pImage->Name = pAssetManager->GetName(ImageDesc.Path);
			pImage->Image = std::move(Image);
			pImage->sRGB = ImageDesc.sRGB;

			ImageDesc.Callback(pImage);

			LOG_INFO("{} Loaded", ImageDesc.Path.string());
		}

		MeshDesc MeshDesc;
		while (MeshDescQueue.Dequeue(MeshDesc, 0))
		{
			const auto Path = MeshDesc.Path.string();

			const aiScene* paiScene = Importer.ReadFile(Path.data(),
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

				// Texture coordinates
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

			std::shared_ptr<Mesh> pMesh = std::make_shared<Mesh>();

			pMesh->Name = pAssetManager->GetName(MeshDesc.Path);
			DirectX::BoundingBox::CreateFromPoints(pMesh->BoundingBox, vertices.size(), &vertices[0].Position, sizeof(Vertex));

			pMesh->VertexCount = vertices.size();
			pMesh->IndexCount = indices.size();

			if (MeshDesc.KeepGeometryInRAM)
			{
				pMesh->Vertices = std::move(vertices);
				pMesh->Indices = std::move(indices);
			}

			MeshDesc.Callback(pMesh);

			LOG_INFO("{} Loaded", MeshDesc.Path.string());
		}
	}

	return EXIT_SUCCESS;
}