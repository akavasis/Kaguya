#include "pch.h"
#include "Light.h"

using namespace DirectX;

DirectionalLight::DirectionalLight()
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
	Lambda = 0.5f;
}

std::array<OrthographicCamera, NUM_CASCADES> DirectionalLight::GenerateCascades(const Camera& Camera, unsigned int Resolution)
{
	// Calculate cascade
	const float maxDistance = 1.0f;
	const float minDistance = 0.0f;
	std::array<OrthographicCamera, NUM_CASCADES> cascadeCameras;

	float cascadeSplits[NUM_CASCADES] = { 0.0f, 0.0f, 0.0f, 0.0f };

	if (const PerspectiveCamera* pPerspectiveCamera = dynamic_cast<const PerspectiveCamera*>(&Camera))
	{
		float nearClip = pPerspectiveCamera->NearZ();
		float farClip = pPerspectiveCamera->FarZ();
		float clipRange = farClip - nearClip;

		float minZ = nearClip + minDistance * clipRange;
		float maxZ = nearClip + maxDistance * clipRange;

		float range = maxZ - minZ;
		float ratio = maxZ / minZ;

		for (int i = 0; i < NUM_CASCADES; ++i)
		{
			float p = (i + 1) / static_cast<float>(NUM_CASCADES);
			float log = minZ * std::pow(ratio, p);
			float uniform = minZ + range * p;
			float d = Lambda * (log - uniform) + uniform;
			cascadeSplits[i] = (d - nearClip) / clipRange;
		}
	}
	else
	{
		for (int i = 0; i < NUM_CASCADES; ++i)
		{
			cascadeSplits[i] = Math::Lerp(minDistance, maxDistance, (i + 1.0f) / float(NUM_CASCADES));
		}
	}

	// Calculate projection matrix for each cascade
	for (int cascadeIndex = 0; cascadeIndex < NUM_CASCADES; ++cascadeIndex)
	{
		XMVECTOR frustumCornersWS[8] =
		{
			XMVectorSet(-1.0f, +1.0f, 0.0f, 1.0f),	// Near top left
			XMVectorSet(+1.0f, +1.0f, 0.0f, 1.0f),	// Near top right
			XMVectorSet(+1.0f, -1.0f, 0.0f, 1.0f),	// Near bottom right
			XMVectorSet(-1.0f, -1.0f, 0.0f, 1.0f),	// Near bottom left
			XMVectorSet(-1.0f, +1.0f, 1.0f, 1.0f),  // Far top left
			XMVectorSet(+1.0f, +1.0f, 1.0f, 1.0f),	// Far top right
			XMVectorSet(+1.0f, -1.0f, 1.0f, 1.0f),	// Far bottom right
			XMVectorSet(-1.0f, -1.0f, 1.0f, 1.0f)	// Far bottom left
		};

		// Used for unprojecting
		XMMATRIX invCameraViewProj = XMMatrixInverse(nullptr, Camera.ViewProjectionMatrix());
		for (int i = 0; i < 8; ++i)
			frustumCornersWS[i] = XMVector3TransformCoord(frustumCornersWS[i], invCameraViewProj);

		float prevCascadeSplit = cascadeIndex == 0 ? 0.0f : cascadeSplits[cascadeIndex - 1];
		float cascadeSplit = cascadeSplits[cascadeIndex];

		// Calculate frustum corners of current cascade
		for (int i = 0; i < 4; ++i)
		{
			XMVECTOR cornerRay = frustumCornersWS[i + 4] - frustumCornersWS[i];
			XMVECTOR nearCornerRay = cornerRay * prevCascadeSplit;
			XMVECTOR farCornerRay = cornerRay * cascadeSplit;
			frustumCornersWS[i + 4] = frustumCornersWS[i] + farCornerRay;
			frustumCornersWS[i] = frustumCornersWS[i] + nearCornerRay;
		}

		XMVECTOR vCenter = XMVectorZero();
		for (int i = 0; i < 8; ++i)
			vCenter += frustumCornersWS[i];
		vCenter *= (1.0f / 8.0f);

		XMVECTOR vRadius = XMVectorZero();
		for (int i = 0; i < 8; ++i)
		{
			XMVECTOR distance = XMVector3Length(frustumCornersWS[i] - vCenter);
			vRadius = XMVectorMax(vRadius, distance);
		}
		float radius = std::ceilf(XMVectorGetX(vRadius) * 16.0f) / 16.0f;
		float scaledRadius = radius * ((static_cast<float>(Resolution) + 7.0f) / static_cast<float>(Resolution));

		// Negate direction
		XMVECTOR lightPos = vCenter + (-XMLoadFloat3(&Direction) * radius);

		cascadeCameras[cascadeIndex].SetLens(-scaledRadius, +scaledRadius, -scaledRadius, +scaledRadius, 0.0f, radius * 2.0f);
		cascadeCameras[cascadeIndex].SetLookAt(lightPos, vCenter, Math::Up);

		// Create the rounding matrix, by projecting the world-space origin and determining
		// the fractional offset in texel space
		DirectX::XMMATRIX shadowMatrix = cascadeCameras[cascadeIndex].ViewProjectionMatrix();
		DirectX::XMVECTOR shadowOrigin = DirectX::XMVectorSet(0.0f, 0.0f, 0.0f, 1.0f);
		shadowOrigin = DirectX::XMVector4Transform(shadowOrigin, shadowMatrix);
		shadowOrigin = DirectX::XMVectorScale(shadowOrigin, static_cast<float>(Resolution) * 0.5f);

		DirectX::XMVECTOR roundedOrigin = DirectX::XMVectorRound(shadowOrigin);
		DirectX::XMVECTOR roundOffset = DirectX::XMVectorSubtract(roundedOrigin, shadowOrigin);
		roundOffset = DirectX::XMVectorScale(roundOffset, 2.0f / static_cast<float>(Resolution));
		roundOffset = DirectX::XMVectorSetZ(roundOffset, 0.0f);
		roundOffset = DirectX::XMVectorSetW(roundOffset, 0.0f);

		DirectX::XMMATRIX shadowProj = cascadeCameras[cascadeIndex].ProjectionMatrix();
		shadowProj.r[3] = DirectX::XMVectorAdd(shadowProj.r[3], roundOffset);
		cascadeCameras[cascadeIndex].SetProjection(shadowProj);

		// Transform NDC space [-1,+1]^2 to texture space [0,1]^2
		XMMATRIX T(0.5f, 0.0f, 0.0f, 0.0f,
			0.0f, -0.5f, 0.0f, 0.0f,
			0.0f, 0.0f, 1.0f, 0.0f,
			0.5f, 0.5f, 0.0f, 1.0f);
		XMMATRIX lightViewProj = cascadeCameras[cascadeIndex].ViewProjectionMatrix();
		XMMATRIX shadowTransform = lightViewProj * T;
		const float clipDist = Camera.FarZ() - Camera.NearZ();

		// Update cascade
		XMStoreFloat4x4(&Cascades[cascadeIndex].ShadowTransform, shadowTransform);
		Cascades[cascadeIndex].Split = Camera.NearZ() + cascadeSplit * clipDist;
	}

	return cascadeCameras;
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