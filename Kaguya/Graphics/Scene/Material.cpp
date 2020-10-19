#include "pch.h"
#include "Material.h"

void Material::RenderGui()
{
	if (ImGui::TreeNode("Material"))
	{
		if (Textures[AlbedoIdx].Path.empty())		ImGui::ColorEdit3("Albedo", &Albedo.x);
		ImGui::ColorEdit3("Specular", &Specular.x);
		if (Textures[RoughnessIdx].Path.empty())	ImGui::SliderFloat("Roughness", &Roughness, 0.0f, 1.0f);

		ImGui::TreePop();
	}
}