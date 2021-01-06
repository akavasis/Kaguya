#include "pch.h"
#include "Light.h"

PolygonalLight::PolygonalLight(const std::string& Name)
	: Name(Name),
	Dirty(true)
{
	Transform = {};
	Color = { 1.0f, 1.0f, 1.0f };
	SetDimension(1.0f, 1.0f);
	SetLuminousPower(1000.0f);
}

void PolygonalLight::RenderGui()
{
	if (ImGui::TreeNode(Name.data()))
	{
		Dirty |= ImGui::DragFloat3("Position", &Transform.Position.x, 0.01f, -Math::Infinity, Math::Infinity);

		//Dirty |= ImGui::DragFloat3("Orientation", &EulerAngles.x, 0.01f, -Math::Infinity, Math::Infinity);
		//XMStoreFloat4(&Transform.Orientation, DirectX::XMQuaternionRotationRollPitchYawFromVector(XMLoadFloat3(&EulerAngles)));

		Dirty |= ImGui::ColorEdit3("Color", &Color.x);
		Dirty |= ImGui::DragFloat("Luminous Power", &LuminousPower, 5, 0, Math::Infinity, "%.3f lm");
		Dirty |= ImGui::DragFloat2("Dimension", &Width, 0.5f, 1, 50);
		if (Dirty)
		{
			SetDimension(Width, Height);
			SetLuminousPower(LuminousPower);
		}

		ImGui::TreePop();
	}
}

void PolygonalLight::SetDimension(float Width, float Height)
{
	this->Width = Width;
	this->Height = Height;
	Area = Width * Height;
}

void PolygonalLight::SetLuminousPower(Lumen LuminousPower)
{
	this->LuminousPower = LuminousPower;

	// The luminance due to a point on a Lambertian emitter, emitted in any direction, is equal to its total
	// luminous power divided by the emitter area A and the projected solid angle (Pi)
	Luminance = LuminousPower / (Area * DirectX::XM_PI);
}