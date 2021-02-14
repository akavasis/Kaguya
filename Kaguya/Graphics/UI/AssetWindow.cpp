#include "pch.h"
#include "AssetWindow.h"

#include <Graphics/AssetManager.h>

void AssetWindow::RenderGui()
{
	ImGuiIO& IO = ImGui::GetIO();

	ImGui::BeginChild("Asset##Editor", ImVec2(IO.DisplaySize.x * 0.5f, 0), true);
	ImGui::Text("Asset");
	ImGui::Separator();
	ImGui::Spacing();

	UIWindow::Update();

	if (ImGui::BeginPopupContextWindow(nullptr, ImGuiPopupFlags_MouseButtonRight))
	{
		if (ImGui::MenuItem("Import Texture"))
		{
			OpenDialog("dds,tga,hdr", "", [&](auto Path)
			{
				AssetManager::Instance().AsyncLoadImage(Path, false);
			});
		}

		if (ImGui::MenuItem("Import Mesh"))
		{
			OpenDialog("dds,tga,hdr", "", [&](auto Path)
			{
				AssetManager::Instance().AsyncLoadMesh(Path, true);
			});
		}

		ImGui::EndPopup();
	}

	auto& MeshCache = AssetManager::Instance().MeshCache;
	ScopedWriteLock SWL(MeshCache.RWLock);
	MeshCache.Each([&](UINT64 Key, AssetHandle<Asset::Mesh> Resource)
	{
		ImGui::Button(Resource->Name.data());

		// Our buttons are both drag sources and drag targets here!
		if (ImGui::BeginDragDropSource(ImGuiDragDropFlags_None))
		{
			// Set payload to carry the index of our item (could be anything)
			ImGui::SetDragDropPayload("ASSET_MESH", &Key, sizeof(UINT64));

			ImGui::EndDragDropSource();
		}

		bool Delete = false;
		if (ImGui::BeginPopupContextItem())
		{
			if (ImGui::MenuItem("Delete"))
			{
				Delete = true;
			}

			ImGui::EndPopup();
		}

		if (Delete)
		{
			if (auto it = MeshCache.Cache.find(Key);
				it != MeshCache.Cache.end())
			{
				MeshCache.Cache.erase(it);
			}
		}
	});
	ImGui::EndChild();
}