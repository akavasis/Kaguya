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
			T& Component = Entity.AddComponent<T>();
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

			auto Handle = AssetManager::Instance().GetMeshCache().Load(Component.MeshID);

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
					Component.MeshID = (*(UINT64*)payload->Data);

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

			ImGui::EndPopup();
		}
	}
	ImGui::EndChild();
}