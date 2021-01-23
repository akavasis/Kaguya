#include "pch.h"
#include "Editor.h"

#include <type_traits>
#include <imgui_internal.h>

#include "Scene/SceneSerializer.h"

using namespace DirectX;

void HierarchyWindow::RenderGui()
{
	if (ImGui::Begin("Hierarchy"))
	{
		pScene->Registry.each([&](auto Handle)
		{
			Entity Entity(Handle, pScene);

			auto& Name = Entity.GetComponent<Tag>().Name;

			ImGuiTreeNodeFlags flags = ((SelectedEntity == Entity) ? ImGuiTreeNodeFlags_Selected : 0);
			flags |= ImGuiTreeNodeFlags_SpanAvailWidth;
			bool opened = ImGui::TreeNodeEx((void*)(uint64_t)(uint32_t)Entity, flags, Name.data());
			if (ImGui::IsItemClicked())
			{
				SelectedEntity = Entity;
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

			if (opened)
			{
				ImGui::TreePop();
			}

			if (Delete)
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
		if (ImGui::BeginPopupContextWindow("Hierarchy Popup Context Window", ImGuiPopupFlags_MouseButtonRight | ImGuiPopupFlags_NoOpenOverItems))
		{
			if (ImGui::MenuItem("Create Empty"))
			{
				pScene->CreateEntity("GameObject");
			}

			if (ImGui::MenuItem("Serialize"))
			{
				SceneSerializer SceneSerializer(pScene);
				SceneSerializer.Serialize(Application::ExecutableFolderPath / "Scene.yaml");
			}

			if (ImGui::MenuItem("Deserialize"))
			{
				nfdchar_t* outPath = nullptr;
				nfdresult_t result = NFD_OpenDialog("yaml", nullptr, &outPath);

				if (result == NFD_OKAY)
				{
					std::filesystem::path Path = outPath;

					SceneSerializer SceneSerializer(pScene);
					*pScene = SceneSerializer.Deserialize(Path);

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
	}
	ImGui::End();
}

template<class T>
concept IsAComponent = std::is_base_of<Component, T>::value;

template<IsAComponent T>
static void RenderComponent(const char* pName, Entity Entity, bool(*UIFunction)(T&))
{
	// Component filters
	constexpr bool IsTagComponent = std::is_same<T, Tag>::value;
	constexpr bool IsTransformComponent = std::is_same<T, Transform>::value;
	constexpr bool IsAnyFilteredComponent = IsTagComponent || IsTransformComponent;

	if (Entity.HasComponent<T>())
	{
		auto& Component = Entity.GetComponent<T>();

		if constexpr (IsAnyFilteredComponent)
		{
			Component.IsEdited = UIFunction(Component);
		}
		else
		{
			ImVec2 contentRegionAvailable = ImGui::GetContentRegionAvail();

			const ImGuiTreeNodeFlags TreeNodeFlags = ImGuiTreeNodeFlags_DefaultOpen |
				ImGuiTreeNodeFlags_Framed |
				ImGuiTreeNodeFlags_SpanAvailWidth |
				ImGuiTreeNodeFlags_AllowItemOverlap |
				ImGuiTreeNodeFlags_FramePadding;

			ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2{ 4, 4 });
			float lineHeight = GImGui->Font->FontSize + GImGui->Style.FramePadding.y * 2.0f;
			ImGui::Separator();
			bool Collapsed = ImGui::TreeNodeEx((void*)typeid(T).hash_code(), TreeNodeFlags, pName);
			ImGui::PopStyleVar();

			ImGui::SameLine(contentRegionAvailable.x - lineHeight * 0.5f);
			if (ImGui::Button("+", ImVec2{ lineHeight, lineHeight }))
			{
				ImGui::OpenPopup("Component Settings");
			}

			bool RemoveComponent = false;
			if (ImGui::BeginPopup("Component Settings"))
			{
				if (ImGui::MenuItem("Remove component"))
				{
					RemoveComponent = true;
				}

				ImGui::EndPopup();
			}

			if (Collapsed)
			{
				Component.IsEdited = UIFunction(Component);
				ImGui::TreePop();
			}

			if (RemoveComponent)
			{
				Entity.RemoveComponent<T>();
			}
		}
	}
}

template<IsAComponent T>
static void AddNewComponent(const char* pName, Entity Entity)
{
	if (ImGui::MenuItem(pName))
	{
		if (Entity.HasComponent<T>())
		{
			MessageBoxA(nullptr, "Cannot add existing component", "Add Component Error", MB_OK | MB_ICONERROR | MB_DEFAULT_DESKTOP_ONLY);
		}
		else
		{
			Entity.AddComponent<T>();
		}
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
			RenderComponent<Tag>("Tag", SelectedEntity, [](Tag& Tag)
			{
				char Buffer[MAX_PATH] = {};
				memcpy(Buffer, Tag.Name.data(), MAX_PATH);
				if (ImGui::InputText("Tag", Buffer, MAX_PATH))
				{
					Tag.Name = Buffer;
				}

				return false;
			});

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

				return IsEdited;
			});

			RenderComponent<MeshFilter>("Mesh Filter", SelectedEntity, [](MeshFilter& MeshFilter)
			{
				bool IsEdited = false;

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

							IsEdited = true;
						}
						ImGui::EndDragDropTarget();
					}
				}

				return IsEdited;
			});

			RenderComponent<MeshRenderer>("Mesh Renderer", SelectedEntity, [](MeshRenderer& MeshRenderer)
			{
				bool IsEdited = false;

				if (ImGui::TreeNode("Material"))
				{
					auto& Material = MeshRenderer.Material;

					ImGui::Text("Attributes");

					const char* MaterialModels[] = { "Lambertian", "Glossy", "Metal", "Dielectric", "Light" };
					IsEdited |= ImGui::Combo("Model", &Material.Model, MaterialModels, ARRAYSIZE(MaterialModels), ARRAYSIZE(MaterialModels));
					IsEdited |= ImGui::Checkbox("Use Attributes", &Material.UseAttributes);

					switch (Material.Model)
					{
					case LambertianModel:
						IsEdited |= ImGui::ColorEdit3("Albedo", &Material.Albedo.x);
						break;
					case GlossyModel:
						IsEdited |= ImGui::ColorEdit3("Albedo", &Material.Albedo.x);
						IsEdited |= ImGui::SliderFloat("Specular Chance", &Material.SpecularChance, 0, 1);
						IsEdited |= ImGui::SliderFloat("Roughness", &Material.Roughness, 0, 1);
						IsEdited |= ImGui::ColorEdit3("Specular", &Material.Specular.x);
						break;
					case MetalModel:
						IsEdited |= ImGui::ColorEdit3("Albedo", &Material.Albedo.x);
						IsEdited |= ImGui::SliderFloat("Fuzziness", &Material.Fuzziness, 0, 1);
						break;
					case DielectricModel:
						IsEdited |= ImGui::SliderFloat("Index Of Refraction", &Material.IndexOfRefraction, 1, 3);
						IsEdited |= ImGui::ColorEdit3("Specular", &Material.Specular.x);
						IsEdited |= ImGui::ColorEdit3("Refraction", &Material.Refraction.x);
						break;
					case DiffuseLightModel:
						IsEdited |= ImGui::DragFloat3("Emissive", &Material.Emissive.x, 0.25f, 0, 10000);
						break;
					default:
						break;
					}

					ImGui::Text("");
					ImGui::Text("Albedo Map: %s", Material.Textures[AlbedoIdx].Path.empty() ? "[NULL]" : Material.Textures[AlbedoIdx].Path.data());
					ImGui::Text("Normal Map: %s", Material.Textures[NormalIdx].Path.empty() ? "[NULL]" : Material.Textures[NormalIdx].Path.data());
					ImGui::Text("Roughness Map: %s", Material.Textures[RoughnessIdx].Path.empty() ? "[NULL]" : Material.Textures[RoughnessIdx].Path.data());
					ImGui::Text("Metallic Map: %s", Material.Textures[MetallicIdx].Path.empty() ? "[NULL]" : Material.Textures[MetallicIdx].Path.data());
					ImGui::Text("Emissive Map: %s", Material.Textures[EmissiveIdx].Path.empty() ? "[NULL]" : Material.Textures[EmissiveIdx].Path.data());

					ImGui::TreePop();
				}

				return IsEdited;
			});

			if (ImGui::Button("Add Component"))
			{
				ImGui::OpenPopup("Component List");
			}

			if (ImGui::BeginPopup("Component List"))
			{
				AddNewComponent<MeshFilter>("Mesh Filter", SelectedEntity);
				AddNewComponent<MeshRenderer>("Mesh Renderer", SelectedEntity);

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