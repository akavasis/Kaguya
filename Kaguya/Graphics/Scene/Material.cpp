#include "pch.h"
#include "Material.h"

Material::Material(const std::string& Name)
	: Name(Name),
	Dirty(false)
{
	Albedo					= { 0.0f, 0.0f, 0.0f };
	Emissive				= { 0.0f, 0.0f, 0.0f };
	Specular				= { 0.0f, 0.0f, 0.0f };
	Refraction				= { 0.0f, 0.0f, 0.0f };
	SpecularChance			= 0.0f;
	Roughness				= 1.0f;
	Metallic				= 0.0f;
	Fuzziness				= 0.0f;
	IndexOfRefraction		= 1.0f;
	Model					= 0;
	UseAttributes			= false;

	for (int i = 0; i < NumTextureTypes; ++i)
	{
		TextureIndices[i]	= 0;
		TextureChannel[i]	= 0;
		Textures[i]			= {};
	}
}

void Material::RenderGui()
{
	if (ImGui::TreeNode(Name.data()))
	{
		ImGui::Text("Attributes");
		Dirty |= (int)ImGui::ColorEdit3("Albedo", &Albedo.x);
		Dirty |= (int)ImGui::DragFloat3("Emissive", &Emissive.x, 0.5f, 0, 10000);
		Dirty |= (int)ImGui::ColorEdit3("Specular", &Specular.x);
		Dirty |= (int)ImGui::ColorEdit3("Refraction", &Refraction.x);
		Dirty |= (int)ImGui::SliderFloat("Specular Chance", &SpecularChance, 0, 1);
		Dirty |= (int)ImGui::SliderFloat("Roughness", &Roughness, 0, 1);
		Dirty |= (int)ImGui::SliderFloat("Metallic", &Metallic, 0, 1);
		Dirty |= (int)ImGui::SliderFloat("Fuzziness", &Fuzziness, 0, 1);
		Dirty |= (int)ImGui::SliderFloat("Index Of Refraction", &IndexOfRefraction, 1, 3);

		const char* MaterialModels[] = { "Lambertian", "Glossy", "Metal", "Dielectric", "Light" };
		Dirty |= (int)ImGui::Combo("Model", &Model, MaterialModels, ARRAYSIZE(MaterialModels), ARRAYSIZE(MaterialModels));
		Dirty |= (int)ImGui::Checkbox("Use Attributes", &UseAttributes);

		ImGui::Text("");
		ImGui::Text("Albedo Map: %s", Textures[AlbedoIdx].Path.empty() ? "[NULL]" : Textures[AlbedoIdx].Path.data());
		ImGui::Text("Normal Map: %s", Textures[NormalIdx].Path.empty() ? "[NULL]" : Textures[NormalIdx].Path.data());
		ImGui::Text("Roughness Map: %s", Textures[RoughnessIdx].Path.empty() ? "[NULL]" : Textures[RoughnessIdx].Path.data());
		ImGui::Text("Metallic Map: %s", Textures[MetallicIdx].Path.empty() ? "[NULL]" : Textures[MetallicIdx].Path.data());
		ImGui::Text("Emissive Map: %s", Textures[EmissiveIdx].Path.empty() ? "[NULL]" : Textures[EmissiveIdx].Path.data());

		ImGui::TreePop();
	}
}