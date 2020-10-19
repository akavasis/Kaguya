#include "pch.h"
#include "Light.h"

void PolygonalLight::RenderGui()
{
	if (ImGui::TreeNode("Polygonal Light"))
	{
		ImGui::DragFloat3("Position", &Transform.Position.x, 0.01f, -Math::Infinity, Math::Infinity);
		ImGui::ColorEdit3("Color", &Color.x);
		ImGui::DragFloat("Intensity", &Intensity, 0.01f, 0.0f, Math::Infinity);

		ImGui::TreePop();
	}
}