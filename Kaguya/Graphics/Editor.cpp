#include "pch.h"
#include "Editor.h"

#include <imgui_internal.h>

using namespace DirectX;

void HierarchyWindow::RenderGui()
{
	if (ImGui::Begin("Hierarchy"))
	{
		pScene->Registry.each([&](auto Handle)
		{
			Entity Entity(Handle, pScene);

			auto& Name = Entity.GetComponent<Tag>().Name;

			ImGuiTreeNodeFlags flags = ((SelectedEntity == Entity) ? ImGuiTreeNodeFlags_Selected : 0) | ImGuiTreeNodeFlags_OpenOnArrow;
			flags |= ImGuiTreeNodeFlags_SpanAvailWidth;
			bool opened = ImGui::TreeNodeEx((void*)(uint64_t)(uint32_t)Entity, flags, Name.data());
			if (ImGui::IsItemClicked())
			{
				SelectedEntity = Entity;
			}

			bool DeleteContext = false;
			if (ImGui::BeginPopupContextItem("Entity", ImGuiPopupFlags_MouseButtonRight))
			{
				if (ImGui::MenuItem("Delete"))
				{
					DeleteContext = true;
				}

				ImGui::EndPopup();
			}

			if (opened)
			{
				ImGuiTreeNodeFlags flags = ImGuiTreeNodeFlags_OpenOnArrow | ImGuiTreeNodeFlags_SpanAvailWidth;
				bool opened = ImGui::TreeNodeEx((void*)9817239, flags, Name.data());
				if (opened)
				{
					ImGui::TreePop();
				}
				ImGui::TreePop();
			}

			if (DeleteContext)
			{
				pScene->DestroyEntity(Entity);
				if (SelectedEntity == Entity)
				{
					SelectedEntity = {};
				}
			}
		});

		if (ImGui::IsMouseDown(0) && ImGui::IsWindowHovered())
		{
			SelectedEntity = {};
		}

		// Right-click on blank space
		if (ImGui::BeginPopupContextWindow(nullptr, ImGuiPopupFlags_MouseButtonRight))
		{
			if (ImGui::MenuItem("Create Empty"))
			{
				pScene->CreateEntity("GameObject");
			}

			ImGui::EndPopup();
		}
	}
	ImGui::End();
}

template<typename T>
static void RenderComponent(const char* pName, Entity Entity, void(*UIFunction)(T&))
{
	// Component filters
	bool IsTransformComponent = typeid(T).hash_code() == typeid(Transform).hash_code();

	const ImGuiTreeNodeFlags treeNodeFlags = ImGuiTreeNodeFlags_DefaultOpen | ImGuiTreeNodeFlags_Framed | ImGuiTreeNodeFlags_SpanAvailWidth | ImGuiTreeNodeFlags_AllowItemOverlap | ImGuiTreeNodeFlags_FramePadding;
	if (Entity.HasComponent<T>())
	{
		auto& Component = Entity.GetComponent<T>();

		ImVec2 contentRegionAvailable = ImGui::GetContentRegionAvail();

		ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2{ 4, 4 });
		float lineHeight = GImGui->Font->FontSize + GImGui->Style.FramePadding.y * 2.0f;
		ImGui::Separator();
		bool Collapsed = ImGui::TreeNodeEx((void*)typeid(T).hash_code(), treeNodeFlags, pName);
		ImGui::PopStyleVar();

		ImGui::SameLine(contentRegionAvailable.x - lineHeight * 0.5f);
		if (ImGui::Button("+", ImVec2{ lineHeight, lineHeight }))
		{
			ImGui::OpenPopup("ComponentSettings");
		}

		bool RemoveComponent = false;
		if (ImGui::BeginPopup("ComponentSettings"))
		{
			if (!IsTransformComponent && ImGui::MenuItem("Remove component"))
			{
				RemoveComponent = true;
			}

			ImGui::EndPopup();
		}

		if (Collapsed)
		{
			UIFunction(Component);
			ImGui::TreePop();
		}

		if (RemoveComponent)
			Entity.RemoveComponent<T>();
	}
}

void InspectorWindow::SetContext(Entity Entity)
{
	SelectedEntity = Entity;
}

void InspectorWindow::RenderGui()
{
	if (ImGui::Begin("Inspector"))
	{
		if (SelectedEntity)
		{
			auto& TagComponent = SelectedEntity.GetComponent<Tag>();

			char Buffer[MAX_PATH] = {};
			memcpy(Buffer, TagComponent.Name.data(), MAX_PATH);
			if (ImGui::InputText("Tag", Buffer, MAX_PATH))
			{
				TagComponent.Name = Buffer;
			}

			RenderComponent<Transform>("Transform", SelectedEntity, [](Transform& Transform)
			{
				bool IsEdited = false;
				DirectX::XMFLOAT4X4 World;

				// Dont transpose this
				XMStoreFloat4x4(&World, Transform.Matrix());

				float matrixTranslation[3], matrixRotation[3], matrixScale[3];
				ImGuizmo::DecomposeMatrixToComponents(reinterpret_cast<float*>(&World), matrixTranslation, matrixRotation, matrixScale);
				IsEdited |= ImGui::InputFloat3("Translation", matrixTranslation);
				IsEdited |= ImGui::InputFloat3("Rotation", matrixRotation);
				IsEdited |= ImGui::InputFloat3("Scale", matrixScale);
				ImGuizmo::RecomposeMatrixFromComponents(matrixTranslation, matrixRotation, matrixScale, reinterpret_cast<float*>(&World));

				if (IsEdited)
				{
					XMMATRIX mWorld = XMLoadFloat4x4(&World);
					Transform.SetTransform(mWorld);
				}
			});

			RenderComponent<MeshFilter>("Mesh Filter", SelectedEntity, [](MeshFilter& MeshFilter)
			{
				if (MeshFilter.pMesh)
				{
					ImGui::Button(MeshFilter.pMesh->Name.data());
				}
				else
				{
					ImGui::Button("NULL");
					if (ImGui::BeginDragDropTarget())
					{
						if (const ImGuiPayload* payload = ImGui::AcceptDragDropPayload("ASSET_MESH"))
						{
							IM_ASSERT(payload->DataSize == sizeof(Mesh*));
							auto payload_n = (Mesh*)(*(LONG_PTR*)payload->Data); // Yea, ik, ugly
							MeshFilter.pMesh = payload_n;
						}
						ImGui::EndDragDropTarget();
					}
				}
			});

			RenderComponent<MeshRenderer>("Mesh Renderer", SelectedEntity, [](MeshRenderer& MeshRenderer)
			{
				MeshRenderer.Material.RenderGui();
			});

			if (ImGui::Button("Add Component"))
			{
				ImGui::OpenPopup("New Component");
			}

			if (ImGui::BeginPopup("New Component"))
			{
				if (ImGui::MenuItem("Mesh Filter"))
				{
					SelectedEntity.AddComponent<MeshFilter>();
				}

				if (ImGui::MenuItem("Mesh Renderer"))
				{
					SelectedEntity.AddComponent<MeshRenderer>();
				}

				ImGui::EndPopup();
			}
		}
	}
	ImGui::End();
}

void AssetWindow::RenderGui()
{
	if (ImGui::Begin("Asset"))
	{
		if (ImGui::BeginPopupContextWindow(nullptr, ImGuiPopupFlags_MouseButtonRight))
		{
			if (ImGui::MenuItem("Import Mesh"))
			{
				nfdchar_t* outPath = nullptr;
				nfdresult_t result = NFD_OpenDialog("obj", nullptr, &outPath);

				if (result == NFD_OKAY)
				{
					std::filesystem::path Path = outPath;

					pResourceManager->AsyncLoadMesh(Path, true);

					free(outPath);
				}
				else if (result == NFD_CANCEL)
				{
					UNREFERENCED_PARAMETER(result);
				}
				else
				{
					printf("Error: %s\n", NFD_GetError());
				}
			}

			if (ImGui::MenuItem("Import Texture"))
			{
				nfdchar_t* outPath = nullptr;
				nfdresult_t result = NFD_OpenDialog("dds,tga,hdr", nullptr, &outPath);

				if (result == NFD_OKAY)
				{
					std::filesystem::path Path = outPath;

					pResourceManager->AsyncLoadTexture2D(Path, false);

					free(outPath);
				}
				else if (result == NFD_CANCEL)
				{
					UNREFERENCED_PARAMETER(result);
				}
				else
				{
					printf("Error: %s\n", NFD_GetError());
				}
			}

			ImGui::EndPopup();
		}

		ScopedReadLock SRL(pResourceManager->MeshCacheLock);
		for (auto& iter : pResourceManager->MeshCache)
		{
			ImGui::Button(iter.first.data());

			// Our buttons are both drag sources and drag targets here!
			if (ImGui::BeginDragDropSource(ImGuiDragDropFlags_None))
			{
				LONG_PTR MeshAddr = (LONG_PTR)&iter.second; // Yea ik, ugly
				// Set payload to carry the index of our item (could be anything)
				ImGui::SetDragDropPayload("ASSET_MESH", &MeshAddr, sizeof(MeshAddr));

				ImGui::EndDragDropSource();
			}
		}
	}
	ImGui::End();
}

void Editor::RenderGui()
{
	HierarchyWindow.RenderGui();
	InspectorWindow.SetContext(HierarchyWindow.GetSelectedEntity());
	InspectorWindow.RenderGui();
	AssetWindow.RenderGui();
}