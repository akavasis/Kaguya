#include "pch.h"
#include "InspectorWindow.h"

#include <imgui_internal.h>

#include <Graphics/AssetManager.h>

template<class T, class Component>
concept IsUIFunction = requires(T F, Component C)
{
	{ F(C) } -> std::convertible_to<bool>;
};

template<IsAComponent T, bool IsCoreComponent, IsUIFunction<T> UIFunction>
static void RenderComponent(const char* pName, Entity Entity, UIFunction UI, bool(*UISettingFunction)(T&))
{
	if (Entity.HasComponent<T>())
	{
		auto& Component = Entity.GetComponent<T>();

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
			if constexpr (!IsCoreComponent)
			{
				if (ImGui::MenuItem("Remove component"))
				{
					RemoveComponent = true;
				}
			}

			if (UISettingFunction)
			{
				Component.IsEdited |= UISettingFunction(Component);
			}

			ImGui::EndPopup();
		}

		if (Collapsed)
		{
			Component.IsEdited |= UI(Component);
			ImGui::TreePop();
		}

		if (RemoveComponent)
		{
			Entity.RemoveComponent<T>();
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

static bool RenderFloat3Control(const char* pLabel, float* Float3, float ResetValue = 0.0f, float ColumnWidth = 100.0f)
{
	bool IsEdited = false;

	ImGuiIO& io = ImGui::GetIO();
	auto boldFont = io.Fonts->Fonts[0];

	ImGui::PushID(pLabel);

	ImGui::Columns(2);
	ImGui::SetColumnWidth(0, ColumnWidth);
	ImGui::Text(pLabel);
	ImGui::NextColumn();

	ImGui::PushMultiItemsWidths(3, ImGui::CalcItemWidth());
	ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2{ 0, 0 });

	float lineHeight = GImGui->Font->FontSize + GImGui->Style.FramePadding.y * 2.0f;
	ImVec2 buttonSize = { lineHeight + 3.0f, lineHeight };

	ImGui::PushStyleColor(ImGuiCol_Button, ImVec4{ 0.8f, 0.1f, 0.15f, 1.0f });
	ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4{ 0.9f, 0.2f, 0.2f, 1.0f });
	ImGui::PushStyleColor(ImGuiCol_ButtonActive, ImVec4{ 0.8f, 0.1f, 0.15f, 1.0f });
	ImGui::PushFont(boldFont);
	if (ImGui::Button("X", buttonSize))
	{
		Float3[0] = ResetValue;
		IsEdited |= true;
	}
	ImGui::PopFont();
	ImGui::PopStyleColor(3);

	ImGui::SameLine();
	IsEdited |= ImGui::DragFloat("##X", &Float3[0], 0.1f, 0.0f, 0.0f, "%.3f");
	ImGui::PopItemWidth();
	ImGui::SameLine();

	ImGui::PushStyleColor(ImGuiCol_Button, ImVec4{ 0.2f, 0.7f, 0.2f, 1.0f });
	ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4{ 0.3f, 0.8f, 0.3f, 1.0f });
	ImGui::PushStyleColor(ImGuiCol_ButtonActive, ImVec4{ 0.2f, 0.7f, 0.2f, 1.0f });
	ImGui::PushFont(boldFont);
	if (ImGui::Button("Y", buttonSize))
	{
		Float3[1] = ResetValue;
		IsEdited |= true;
	}
	ImGui::PopFont();
	ImGui::PopStyleColor(3);

	ImGui::SameLine();
	IsEdited |= ImGui::DragFloat("##Y", &Float3[1], 0.1f, 0.0f, 0.0f, "%.3f");
	ImGui::PopItemWidth();
	ImGui::SameLine();

	ImGui::PushStyleColor(ImGuiCol_Button, ImVec4{ 0.1f, 0.25f, 0.8f, 1.0f });
	ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4{ 0.2f, 0.35f, 0.9f, 1.0f });
	ImGui::PushStyleColor(ImGuiCol_ButtonActive, ImVec4{ 0.1f, 0.25f, 0.8f, 1.0f });
	ImGui::PushFont(boldFont);
	if (ImGui::Button("Z", buttonSize))
	{
		Float3[2] = ResetValue;
		IsEdited |= true;
	}
	ImGui::PopFont();
	ImGui::PopStyleColor(3);

	ImGui::SameLine();
	IsEdited |= ImGui::DragFloat("##Z", &Float3[2], 0.1f, 0.0f, 0.0f, "%.3f");
	ImGui::PopItemWidth();

	ImGui::PopStyleVar();

	ImGui::Columns(1);

	ImGui::PopID();

	return IsEdited;
}

static bool EditTransform(
	Transform& Transform,
	const float* pCameraView,
	float* pCameraProjection,
	float* pMatrix)
{
	bool IsEdited = false;

	static float				Snap[3] = { 1, 1, 1 };
	static ImGuizmo::MODE		CurrentGizmoMode = ImGuizmo::WORLD;

	ImGui::Text("Operation");

	if (ImGui::RadioButton("Translate", Transform.CurrentGizmoOperation == ImGuizmo::TRANSLATE))
		Transform.CurrentGizmoOperation = ImGuizmo::TRANSLATE;
	ImGui::SameLine();

	if (ImGui::RadioButton("Rotate", Transform.CurrentGizmoOperation == ImGuizmo::ROTATE))
		Transform.CurrentGizmoOperation = ImGuizmo::ROTATE;
	ImGui::SameLine();

	if (ImGui::RadioButton("Scale", Transform.CurrentGizmoOperation == ImGuizmo::SCALE))
		Transform.CurrentGizmoOperation = ImGuizmo::SCALE;

	ImGui::Checkbox("Snap", &Transform.UseSnap);
	ImGui::SameLine();

	switch (Transform.CurrentGizmoOperation)
	{
	case ImGuizmo::TRANSLATE:
		ImGui::InputFloat3("Snap", &Snap[0]);
		break;
	case ImGuizmo::ROTATE:
		ImGui::InputFloat("Angle Snap", &Snap[0]);
		break;
	case ImGuizmo::SCALE:
		ImGui::InputFloat("Scale Snap", &Snap[0]);
		break;
	}

	IsEdited |= ImGuizmo::Manipulate(
		pCameraView, pCameraProjection,
		(ImGuizmo::OPERATION)Transform.CurrentGizmoOperation, CurrentGizmoMode,
		pMatrix, nullptr,
		Transform.UseSnap ? Snap : nullptr);

	return IsEdited;
}

void InspectorWindow::RenderGui(float x, float y, float width, float height)
{
	ImGuiIO& IO = ImGui::GetIO();

	ImGui::BeginChild("Inspector##Editor", ImVec2(0, 0), true);
	ImGui::Text("Inspector");
	ImGui::Separator();
	ImGui::Spacing();

	UIWindow::Update();

	if (SelectedEntity)
	{
		RenderComponent<Tag, true>("Tag", SelectedEntity,
			[](Tag& Component)
		{
			char Buffer[MAX_PATH] = {};
			memcpy(Buffer, Component.Name.data(), MAX_PATH);
			if (ImGui::InputText("Tag", Buffer, MAX_PATH))
			{
				Component.Name = Buffer;
			}

			return false;
		},
			nullptr);

		RenderComponent<Transform, true>("Transform", SelectedEntity,
			[&](Transform& Component)
		{
			bool IsEdited = false;

			DirectX::XMFLOAT4X4 World;
			DirectX::XMFLOAT4X4 View, Projection;

			// Dont transpose this
			XMStoreFloat4x4(&World, Component.Matrix());

			float matrixTranslation[3], matrixRotation[3], matrixScale[3];
			ImGuizmo::DecomposeMatrixToComponents(reinterpret_cast<float*>(&World), matrixTranslation, matrixRotation, matrixScale);
			IsEdited |= RenderFloat3Control("Translation", matrixTranslation);
			IsEdited |= RenderFloat3Control("Rotation", matrixRotation);
			IsEdited |= RenderFloat3Control("Scale", matrixScale, 1);
			ImGuizmo::RecomposeMatrixFromComponents(matrixTranslation, matrixRotation, matrixScale, reinterpret_cast<float*>(&World));

			// Dont transpose this
			XMStoreFloat4x4(&View, pScene->Camera.ViewMatrix());
			XMStoreFloat4x4(&Projection, pScene->Camera.ProjectionMatrix());

			ImGuizmo::SetRect(x, y, width, height);
			ImGui::Separator();

			// I'm not sure what this does, I put it here because the example does it...
			ImGuizmo::SetID(0);

			// If we have edited the transform, update it and mark it as dirty so it will be updated on the GPU side
			IsEdited |= EditTransform(
				Component,
				reinterpret_cast<float*>(&View),
				reinterpret_cast<float*>(&Projection),
				reinterpret_cast<float*>(&World));

			if (IsEdited)
			{
				DirectX::XMMATRIX mWorld = XMLoadFloat4x4(&World);
				Component.SetTransform(mWorld);
			}

			return IsEdited;
		},
			[](auto Component)
		{
			bool IsEdited = false;

			if (IsEdited |= ImGui::MenuItem("Reset"))
			{
				Component = Transform();
			}

			return IsEdited;
		});

		RenderComponent<MeshFilter, false>("Mesh Filter", SelectedEntity,
			[&](MeshFilter& Component)
		{
			bool IsEdited = false;

			auto Handle = AssetManager::Instance().GetMeshCache().Load(Component.Key);

			ImGui::Text("Mesh: ");
			ImGui::SameLine();
			if (Handle)
			{
				ImGui::Button(Handle->Name.data());
			}
			else
			{
				ImGui::Button("NULL");
			}

			if (ImGui::BeginDragDropTarget())
			{
				if (const ImGuiPayload* payload = ImGui::AcceptDragDropPayload("ASSET_MESH");
					payload)
				{
					IM_ASSERT(payload->DataSize == sizeof(UINT64));
					Component.Key = (*(UINT64*)payload->Data);

					IsEdited = true;
				}
				ImGui::EndDragDropTarget();
			}

			return IsEdited;
		},
			nullptr);

		RenderComponent<MeshRenderer, false>("Mesh Renderer", SelectedEntity,
			[](MeshRenderer& Component)
		{
			bool IsEdited = false;

			if (ImGui::TreeNode("Material"))
			{
				auto& Material = Component.Material;

				ImGui::Text("Attributes");

				const char* BSDFTypes[BSDFTypes::NumBSDFTypes] = { "Lambertian", "Mirror", "Glass", "Disney" };
				IsEdited |= ImGui::Combo("Type", &Material.BSDFType, BSDFTypes, ARRAYSIZE(BSDFTypes), ARRAYSIZE(BSDFTypes));

				switch (Material.BSDFType)
				{
				case BSDFTypes::Lambertian:
					IsEdited |= RenderFloat3Control("R", &Material.baseColor.x);
					break;
				case BSDFTypes::Mirror:
					IsEdited |= RenderFloat3Control("R", &Material.baseColor.x);
					break;
				case BSDFTypes::Glass:
					IsEdited |= RenderFloat3Control("R", &Material.baseColor.x);
					IsEdited |= RenderFloat3Control("T", &Material.T.x);
					IsEdited |= ImGui::SliderFloat("etaA", &Material.etaA, 1, 3);
					IsEdited |= ImGui::SliderFloat("etaB", &Material.etaB, 1, 3);
					break;
				case BSDFTypes::Disney:
					IsEdited |= RenderFloat3Control("Base Color", &Material.baseColor.x);
					IsEdited |= ImGui::SliderFloat("Metallic", &Material.metallic, 0, 1);
					IsEdited |= ImGui::SliderFloat("Subsurface", &Material.subsurface, 0, 1);
					IsEdited |= ImGui::SliderFloat("Specular", &Material.specular, 0, 1);
					IsEdited |= ImGui::SliderFloat("Roughness", &Material.roughness, 0, 1);
					IsEdited |= ImGui::SliderFloat("SpecularTint", &Material.specularTint, 0, 1);
					IsEdited |= ImGui::SliderFloat("Anisotropic", &Material.anisotropic, 0, 1);
					IsEdited |= ImGui::SliderFloat("Sheen", &Material.sheen, 0, 1);
					IsEdited |= ImGui::SliderFloat("SheenTint", &Material.sheenTint, 0, 1);
					IsEdited |= ImGui::SliderFloat("Clearcoat", &Material.clearcoat, 0, 1);
					IsEdited |= ImGui::SliderFloat("ClearcoatGloss", &Material.clearcoatGloss, 0, 1);
					break;
				default:
					break;
				}

				auto ImageBox = [&](TextureTypes TextureType, UINT64& Key, std::string_view Name)
				{
					auto Handle = AssetManager::Instance().GetImageCache().Load(Key);

					ImGui::Text(Name.data());
					ImGui::SameLine();
					if (Handle)
					{
						ImGui::Button(Handle->Name.data());
						Material.TextureIndices[TextureType] = Handle->SRV.Index;
					}
					else
					{
						ImGui::Button("NULL");
						Material.TextureIndices[TextureType] = -1;
					}

					if (ImGui::BeginDragDropTarget())
					{
						if (const ImGuiPayload* payload = ImGui::AcceptDragDropPayload("ASSET_IMAGE");
							payload)
						{
							IM_ASSERT(payload->DataSize == sizeof(UINT64));
							Key = (*(UINT64*)payload->Data);

							IsEdited = true;
						}
						ImGui::EndDragDropTarget();
					}
				};

				ImageBox(TextureTypes::AlbedoIdx, Material.TextureKeys[0], "Albedo: ");

				ImGui::TreePop();
			}

			return IsEdited;
		},
			nullptr);

		RenderComponent<Light, false>("Light", SelectedEntity,
			[](Light& Component)
		{
			bool IsEdited = false;

			auto pType = (int*)&Component.Type;

			const char* LightTypes[] = { "Point", "Quad" };
			IsEdited |= ImGui::Combo("Type", pType, LightTypes, ARRAYSIZE(LightTypes), ARRAYSIZE(LightTypes));
			IsEdited |= RenderFloat3Control("I", &Component.I.x);
			IsEdited |= ImGui::SliderFloat("Width", &Component.Width, 1, 50);
			IsEdited |= ImGui::SliderFloat("Height", &Component.Height, 1, 50);

			return IsEdited;
		},
			nullptr);

		if (ImGui::Button("Add Component"))
		{
			ImGui::OpenPopup("Component List");
		}

		if (ImGui::BeginPopup("Component List"))
		{
			AddNewComponent<MeshFilter>("Mesh Filter", SelectedEntity);
			AddNewComponent<MeshRenderer>("Mesh Renderer", SelectedEntity);
			AddNewComponent<Light>("Light", SelectedEntity);

			ImGui::EndPopup();
		}
	}
	ImGui::EndChild();
}