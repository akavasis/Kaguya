#include "pch.h"
#include "Light.h"

void PolygonalLight::RenderGui()
{
	if (ImGui::TreeNode("Polygonal Light"))
	{
		ImGui::DragFloat3("Position", &Transform.Position.x, 0.01f, -Math::Infinity, Math::Infinity);
		ImGui::DragFloat3("Color", &Color.x, 0.01f, 0.1f, 10000.0f);

		ImGui::TreePop();
	}
}