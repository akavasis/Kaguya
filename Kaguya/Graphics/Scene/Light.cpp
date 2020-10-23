#include "pch.h"
#include "Light.h"

PolygonalLight::PolygonalLight()
{
	Name			= "";
	Dirty			= false;

	Transform		= {};
	Color			= { 1.0f, 1.0f, 1.0f };
	Intensity		= 1.0f;
	Width			= 1.0f;
	Height			= 1.0f;

	GpuLightIndex	= -1;
}

void PolygonalLight::RenderGui()
{
	if (ImGui::TreeNode(Name.data()))
	{
		Dirty |= ImGui::DragFloat3("Position", &Transform.Position.x, 0.01f, -Math::Infinity, Math::Infinity);
		Dirty |= ImGui::ColorEdit3("Color", &Color.x);
		Dirty |= ImGui::DragFloat("Intensity", &Intensity, 0.01f, 0.0f, Math::Infinity);
		Dirty |= ImGui::DragFloat2("Dimension", &Width, 0.01f, 0.1f, 50.0f);

		ImGui::TreePop();
	}
}