#include "pch.h"
#include "Material.h"

using namespace DirectX;

bool RenderLambertianModelGui(XMFLOAT3* pAlbedo)
{
	return ImGui::ColorEdit3("Albedo", &pAlbedo->x);
}

bool RenderGlossyModelGui(XMFLOAT3* pAlbedo, float* pSpecularChance, float* pRoughness, XMFLOAT3* pSpecular)
{
	bool Dirty = false;
	Dirty |= ImGui::ColorEdit3("Albedo", &pAlbedo->x);
	Dirty |= ImGui::SliderFloat("Specular Chance", pSpecularChance, 0, 1);
	Dirty |= ImGui::SliderFloat("Roughness", pRoughness, 0, 1);
	Dirty |= ImGui::ColorEdit3("Specular", &pSpecular->x);
	return Dirty;
}

bool RenderMetalModelGui(XMFLOAT3* pAlbedo, float* pFuzziness)
{
	bool Dirty = false;
	Dirty |= ImGui::ColorEdit3("Albedo", &pAlbedo->x);
	Dirty |= ImGui::SliderFloat("Fuzziness", pFuzziness, 0, 1);
	return Dirty;
}

bool RenderDielectricModelGui(float* pIndexOfRefraction, XMFLOAT3* pSpecular, XMFLOAT3* pRefraction)
{
	bool Dirty = false;
	Dirty |= ImGui::SliderFloat("Index Of Refraction", pIndexOfRefraction, 1, 3);
	Dirty |= ImGui::ColorEdit3("Specular", &pSpecular->x);
	Dirty |= ImGui::ColorEdit3("Refraction", &pRefraction->x);
	return Dirty;
}

bool RenderDiffuseLightModelGui(XMFLOAT3* pEmissive)
{
	return ImGui::DragFloat3("Emissive", &pEmissive->x, 0.25f, 0, 10000);
}

Material::Material()
	: Dirty(true)
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
	if (ImGui::TreeNode("Material"))
	{
		ImGui::Text("Attributes");

		const char* MaterialModels[] = { "Lambertian", "Glossy", "Metal", "Dielectric", "Light" };
		Dirty |= ImGui::Combo("Model", &Model, MaterialModels, ARRAYSIZE(MaterialModels), ARRAYSIZE(MaterialModels));
		Dirty |= ImGui::Checkbox("Use Attributes", &UseAttributes);

		switch (Model)
		{
		case LambertianModel:
			Dirty |= RenderLambertianModelGui(&Albedo);
			break;
		case GlossyModel:
			Dirty |= RenderGlossyModelGui(&Albedo, &SpecularChance, &Roughness, &Specular);
			break;
		case MetalModel:
			Dirty |= RenderMetalModelGui(&Albedo, &Fuzziness);
			break;
		case DielectricModel:
			Dirty |= RenderDielectricModelGui(&IndexOfRefraction, &Specular, &Refraction);
			break;
		case DiffuseLightModel:
			Dirty |= RenderDiffuseLightModelGui(&Emissive);
			break;
		default:
			break;
		}

		ImGui::Text("");
		ImGui::Text("Albedo Map: %s", Textures[AlbedoIdx].Path.empty() ? "[NULL]" : Textures[AlbedoIdx].Path.data());
		ImGui::Text("Normal Map: %s", Textures[NormalIdx].Path.empty() ? "[NULL]" : Textures[NormalIdx].Path.data());
		ImGui::Text("Roughness Map: %s", Textures[RoughnessIdx].Path.empty() ? "[NULL]" : Textures[RoughnessIdx].Path.data());
		ImGui::Text("Metallic Map: %s", Textures[MetallicIdx].Path.empty() ? "[NULL]" : Textures[MetallicIdx].Path.data());
		ImGui::Text("Emissive Map: %s", Textures[EmissiveIdx].Path.empty() ? "[NULL]" : Textures[EmissiveIdx].Path.data());

		ImGui::TreePop();
	}
}