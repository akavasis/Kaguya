#include "pch.h"
#include "Material.h"

void Material::RenderGui()
{
	if (ImGui::TreeNode(Name.data()))
	{
		ImGui::Text("Albedo Map: %s", Textures[AlbedoIdx].Path.empty() ? "[NULL]" : Textures[AlbedoIdx].Path.filename().string().data());
		ImGui::Text("Normal Map: %s", Textures[NormalIdx].Path.empty() ? "[NULL]" : Textures[NormalIdx].Path.filename().string().data());
		ImGui::Text("Roughness Map: %s", Textures[RoughnessIdx].Path.empty() ? "[NULL]" : Textures[RoughnessIdx].Path.filename().string().data());
		ImGui::Text("Metallic Map: %s", Textures[MetallicIdx].Path.empty() ? "[NULL]" : Textures[MetallicIdx].Path.filename().string().data());
		ImGui::Text("Emissive Map: %s", Textures[EmissiveIdx].Path.empty() ? "[NULL]" : Textures[EmissiveIdx].Path.filename().string().data());

		ImGui::TreePop();
	}
}