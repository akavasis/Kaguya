#include "pch.h"
#include "Material.h"

Material::Material()
{
	Name					= "";
	Dirty					= false;

	Albedo					= { 0.0f, 0.0f, 0.0f };
	Emissive				= { 0.0f, 0.0f, 0.0f };
	Specular				= { 0.0f, 0.0f, 0.0f };
	Refraction				= { 0.0f, 0.0f, 0.0f };
	SpecularChance			= 0.0f;
	Roughness				= 0.0f;
	Fuzziness				= 0.0f;
	IndexOfRefraction		= 1.0f;
	Model					= 0;

	for (int i = 0; i < NumTextureTypes; ++i)
	{
		TextureIndices[i]	= -1;
		Textures[i]			= {};
	}
	GpuMaterialIndex		= 0;
}

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