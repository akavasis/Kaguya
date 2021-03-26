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

	bool addAllToHierarchy = false;
	if (ImGui::BeginPopupContextWindow(nullptr, ImGuiPopupFlags_MouseButtonRight))
	{
		// TODO: Write a function for this
		{
			if (ImGui::Button("Import Texture"))
			{
				ImGui::OpenPopup("Texture Options");
			}

			// Always center this window when appearing
			const ImVec2 center(ImGui::GetIO().DisplaySize.x * 0.5f, ImGui::GetIO().DisplaySize.y * 0.5f);
			ImGui::SetNextWindowPos(center, ImGuiCond_Appearing, ImVec2(0.5f, 0.5f));
			if (ImGui::BeginPopupModal("Texture Options", nullptr, ImGuiWindowFlags_AlwaysAutoResize))
			{
				static bool sRGB = true;
				ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(0, 0));
				ImGui::Checkbox("sRGB", &sRGB);
				ImGui::PopStyleVar();

				if (ImGui::Button("Browse", ImVec2(120, 0)))
				{
					OpenDialog("dds,tga,hdr", "", [&](auto Path)
					{
						AssetManager::Instance().AsyncLoadImage(Path, sRGB);
					});

					ImGui::CloseCurrentPopup();
				}
				ImGui::SetItemDefaultFocus();
				ImGui::SameLine();
				if (ImGui::Button("Cancel", ImVec2(120, 0))) { ImGui::CloseCurrentPopup(); }
				ImGui::EndPopup();
			}
		}

		{
			if (ImGui::Button("Import Mesh"))
			{
				ImGui::OpenPopup("Mesh Options");
			}

			// Always center this window when appearing
			const ImVec2 center(ImGui::GetIO().DisplaySize.x * 0.5f, ImGui::GetIO().DisplaySize.y * 0.5f);
			ImGui::SetNextWindowPos(center, ImGuiCond_Appearing, ImVec2(0.5f, 0.5f));
			if (ImGui::BeginPopupModal("Mesh Options", nullptr, ImGuiWindowFlags_AlwaysAutoResize))
			{
				static bool KeepGeometryInRAM = true;
				ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(0, 0));
				ImGui::Checkbox("Keep Geometry In RAM", &KeepGeometryInRAM);
				ImGui::PopStyleVar();

				if (ImGui::Button("Browse", ImVec2(120, 0)))
				{
					OpenDialogMultiple("obj,stl,ply", "", [&](auto Path)
					{
						AssetManager::Instance().AsyncLoadMesh(Path, KeepGeometryInRAM);
					});

					ImGui::CloseCurrentPopup();
				}
				ImGui::SetItemDefaultFocus();
				ImGui::SameLine();
				if (ImGui::Button("Cancel", ImVec2(120, 0))) { ImGui::CloseCurrentPopup(); }
				ImGui::EndPopup();
			}
		}

		if (ImGui::Button("Add all to hierarchy"))
		{
			addAllToHierarchy = true;
		}

		ImGui::EndPopup();
	}

	{
		auto& ImageCache = AssetManager::Instance().ImageCache;
		ScopedWriteLock SWL(ImageCache.RWLock);

		ImageCache.Each([&](UINT64 Key, AssetHandle<Asset::Image> Resource)
		{
			ImGui::Button(Resource->Name.data());

			// Our buttons are both drag sources and drag targets here!
			if (ImGui::BeginDragDropSource(ImGuiDragDropFlags_None))
			{
				// Set payload to carry the index of our item (could be anything)
				ImGui::SetDragDropPayload("ASSET_IMAGE", &Key, sizeof(UINT64));

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
				if (auto it = ImageCache.Cache.find(Key);
					it != ImageCache.Cache.end())
				{
					ImageCache.Cache.erase(it);
				}
			}
		});
	}

	{
		auto& MeshCache = AssetManager::Instance().MeshCache;
		ScopedWriteLock SWL(MeshCache.RWLock);

		if (addAllToHierarchy)
		{
			MeshCache.Each([&](UINT64 Key, AssetHandle<Asset::Mesh> Resource)
			{
				auto entity = pScene->CreateEntity(Resource->Name);
				MeshFilter& meshFilter = entity.AddComponent<MeshFilter>();
				meshFilter.Key = Key;

				MeshRenderer& meshRenderer = entity.AddComponent<MeshRenderer>();
			});
		}

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
	}

	ImGui::EndChild();
}