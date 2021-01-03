#include "pch.h"
#include "Camera.h"

using namespace DirectX;

static constexpr float s_TranslateSpeed = 15.0f;
static constexpr float s_RotationSpeed = 15.0f;

Camera::Camera()
{
	FoVY				= DirectX::XM_PIDIV4;
	AspectRatio			= 1.0f;
	NearZ				= 0.1f;
	FarZ				= 500.0f;

	FocalLength			= 10.0f;
	RelativeAperture	= 0.0f;
	ShutterTime			= 1.0f / 125.0f;
	SensorSensitivity	= 200.0f;
}

Camera::Camera(float FoVY, float AspectRatio, float NearZ, float FarZ)
	: FoVY(FoVY),
	AspectRatio(AspectRatio),
	NearZ(NearZ),
	FarZ(FarZ)
{

}

DirectX::XMVECTOR Camera::GetUVector() const
{
	return Transform.Right() * FocalLength * tanf(FoVY * 0.5f) * AspectRatio;
}

DirectX::XMVECTOR Camera::GetVVector() const
{
	return Transform.Up() * FocalLength * tanf(FoVY * 0.5f);
}

DirectX::XMVECTOR Camera::GetWVector() const
{
	return Transform.Forward() * FocalLength;
}

EV Camera::ExposureValue100() const
{
	// EV number is defined as:
	// 2^ EV_s = N^2 / t and EV_s = EV_100 + log2 (S /100)
	// This gives
	// EV_s = log2 (N^2 / t)
	// EV_100 + log2 (S /100) = log2 (N^2 / t)
	// EV_100 = log2 (N^2 / t) - log2 (S /100)
	// EV_100 = log2 (N^2 / t . 100 / S)
	return std::log2((RelativeAperture * RelativeAperture) / ShutterTime * 100.0f / SensorSensitivity);
}

DirectX::XMMATRIX Camera::ViewMatrix() const
{
	return XMMatrixInverse(nullptr, Transform.Matrix());
}

DirectX::XMMATRIX Camera::ProjectionMatrix() const
{
	return XMMatrixPerspectiveFovLH(FoVY, AspectRatio, NearZ, FarZ);
}

DirectX::XMMATRIX Camera::ViewProjectionMatrix() const
{
	return XMMatrixMultiply(ViewMatrix(), ProjectionMatrix());
}

DirectX::XMMATRIX Camera::InverseViewMatrix() const
{
	return XMMatrixInverse(nullptr, ViewMatrix());
}

DirectX::XMMATRIX Camera::InverseProjectionMatrix() const
{
	return XMMatrixInverse(nullptr, ProjectionMatrix());
}

DirectX::XMMATRIX Camera::InverseViewProjectionMatrix() const
{
	return XMMatrixInverse(nullptr, ViewProjectionMatrix());
}

void Camera::SetLookAt(DirectX::XMVECTOR EyePosition, DirectX::XMVECTOR FocusPosition, DirectX::XMVECTOR UpDirection)
{
	XMMATRIX view = XMMatrixLookAtLH(EyePosition, FocusPosition, UpDirection);
	Transform.SetTransform(XMMatrixInverse(nullptr, view));
}

void Camera::Translate(float DeltaX, float DeltaY, float DeltaZ)
{
	DeltaX *= s_TranslateSpeed;
	DeltaY *= s_TranslateSpeed;
	DeltaZ *= s_TranslateSpeed;
	Transform.Translate(DeltaX, DeltaY, DeltaZ);
}

void Camera::Rotate(float AngleX, float AngleY)
{
	AngleX = XMConvertToRadians(AngleX * s_RotationSpeed);
	AngleY = XMConvertToRadians(AngleY * s_RotationSpeed);
	Transform.Rotate(AngleX, AngleY, 0.0f);
}