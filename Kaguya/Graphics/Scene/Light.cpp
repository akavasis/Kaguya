#include "pch.h"
#include "Light.h"

void PolygonalLight::RenderGui()
{
	if (ImGui::TreeNode(Name.data()))
	{
		ImGui::DragFloat3("Position", &Transform.Position.x, 0.01f, -Math::Infinity, Math::Infinity);
		ImGui::ColorEdit3("Color", &Color.x);
		ImGui::DragFloat("Intensity", &Intensity, 0.01f, 0.0f, Math::Infinity);
		ImGui::DragFloat2("Dimension", &Width, 0.01f, 0.1f, 50.0f);

		ImGui::TreePop();
	}
}