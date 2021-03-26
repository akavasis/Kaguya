#include "pch.h"
#include "HierarchyWindow.h"

#include <Graphics/Scene/SceneParser.h>

void HierarchyWindow::RenderGui()
{
	ImGuiIO& IO = ImGui::GetIO();

	ImGui::BeginChild("Hierarchy##Editor", ImVec2(IO.DisplaySize.x * 0.15f, IO.DisplaySize.y * 0.7f), true);
	ImGui::Text("Hierarchy");
	ImGui::Separator();
	ImGui::Spacing();

	UIWindow::Update();

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

	// Right-click on blank space
	if (ImGui::BeginPopupContextWindow("Hierarchy Popup Context Window", ImGuiPopupFlags_MouseButtonRight | ImGuiPopupFlags_NoOpenOverItems))
	{
		if (ImGui::MenuItem("Create Empty"))
		{
			pScene->CreateEntity("GameObject");
		}

		if (ImGui::MenuItem("Clear"))
		{
			pScene->Clear();
		}

		if (ImGui::MenuItem("Save"))
		{
			SaveDialog("yaml", "", [&](auto Path)
			{
				if (!Path.has_extension())
				{
					Path += ".yaml";
				}

				SceneParser::Save(Path, pScene);
			});
		}

		if (ImGui::MenuItem("Load"))
		{
			OpenDialog("yaml", "", [&](auto Path)
			{
				SceneParser::Load(Path, pScene);
			});
		}

		ImGui::EndPopup();
	}
	ImGui::EndChild();
}