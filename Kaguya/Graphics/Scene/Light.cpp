#include "pch.h"
#include "Light.h"

using namespace DirectX;

DirectionalLight::DirectionalLight()
	: _padding(0.0f)
{
	Reset();
}

void DirectionalLight::RenderImGuiWindow()
{
	if (ImGui::TreeNode("Directional Light"))
	{
		ImGui::ColorEdit3("Strength", &Strength.x);
		ImGui::SliderFloat("Intensity", &Intensity, 1.0f, 50.0f, "%.1f");

		ImGui::Text("Direction");
		ImGui::SliderFloat("X", &Direction.x, -1.0f, 1.0f, "%.1f");
		ImGui::SliderFloat("Y", &Direction.y, -1.0f, 1.0f, "%.1f");
		ImGui::SliderFloat("Z", &Direction.z, -1.0f, 1.0f, "%.1f");
		XMStoreFloat3(&Direction, XMVector3Normalize(XMLoadFloat3(&Direction)));

		if (ImGui::Button("Reset"))
		{
			Reset();
		}
		ImGui::TreePop();
	}
}

void DirectionalLight::Reset()
{
	Strength = { 1.0f, 1.0f, 1.0f };
	Intensity = 3.0f;
	XMStoreFloat3(&Direction, XMVector3Normalize(XMVectorSet(-0.1f, -0.6f, 0.8f, 0.0f)));
}

PointLight::PointLight()
{
	Reset();
}

void PointLight::RenderImGuiWindow()
{
	if (ImGui::TreeNode("Point Light"))
	{
		ImGui::ColorEdit3("Strength", &Strength.x);
		ImGui::SliderFloat("Intensity", &Intensity, 1.0f, 50.0f, "%.1f");

		ImGui::Text("Position");
		ImGui::SliderFloat("X", &Position.x, -50.0f, 50.0f, "%.1f");
		ImGui::SliderFloat("Y", &Position.y, -50.0f, 50.0f, "%.1f");
		ImGui::SliderFloat("Z", &Position.z, -50.0f, 50.0f, "%.1f");

		ImGui::SliderFloat("Radius", &Radius, 1.0f, 50.0f);

		if (ImGui::Button("Reset"))
		{
			Reset();
		}
		ImGui::TreePop();
	}
}

void PointLight::Reset()
{
	Strength = { 1.0f, 1.0f, 1.0f };
	Intensity = 3.0f;
	Position = { 0.0f, 0.0f, 0.0f };
	Radius = 25.0f;
}

SpotLight::SpotLight()
{
	Reset();
}

void SpotLight::RenderImGuiWindow()
{
	if (ImGui::TreeNode("Spot Light"))
	{
		ImGui::ColorEdit3("Strength", &Strength.x);
		ImGui::SliderFloat("Intensity", &Intensity, 1.0f, 50.0f, "%.1f");

		ImGui::Text("Position");
		ImGui::SliderFloat("X", &Position.x, -50.0f, 50.0f, "%.1f");
		ImGui::SliderFloat("Y", &Position.y, -50.0f, 50.0f, "%.1f");
		ImGui::SliderFloat("Z", &Position.z, -50.0f, 50.0f, "%.1f");

		ImGui::SliderFloat("InnerConeAngle", &InnerConeAngle, 1.0f, 50.0f);

		ImGui::Text("Direction");
		ImGui::SliderFloat("X", &Direction.x, -1.0f, 1.0f, "%.1f");
		ImGui::SliderFloat("Y", &Direction.y, -1.0f, 1.0f, "%.1f");
		ImGui::SliderFloat("Z", &Direction.z, -1.0f, 1.0f, "%.1f");
		XMStoreFloat3(&Direction, XMVector3Normalize(XMLoadFloat3(&Direction)));

		ImGui::SliderFloat("OuterConeAngle", &OuterConeAngle, 1.0f, 50.0f);

		ImGui::SliderFloat("Radius", &Radius, 1.0f, 50.0f);

		if (ImGui::Button("Reset"))
		{
			Reset();
		}
		ImGui::TreePop();
	}
}

void SpotLight::Reset()
{
	Strength = { 1.0f, 1.0f, 1.0f };
	Intensity = 3.0f;
	Position = { 0.0f, 0.0f, 0.0f };
	InnerConeAngle = cosf(XMConvertToRadians(2.0f));
	Direction = { 0.0f, -1.0f, 0.0f };
	OuterConeAngle = cosf(XMConvertToRadians(25.0f));
	Radius = 500.0f;
}