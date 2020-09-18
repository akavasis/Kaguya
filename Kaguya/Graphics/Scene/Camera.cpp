#include "pch.h"
#include "Camera.h"
using namespace DirectX;

Camera::Camera()
{
	m_NearZ = m_FarZ = 0.0f;
	m_Projection = Math::Identity();
}

Camera::Camera(float NearZ, float FarZ)
	: m_NearZ(NearZ), m_FarZ(FarZ)
{
	m_Projection = Math::Identity();
}

XMMATRIX Camera::ViewMatrix() const
{
	return XMMatrixInverse(nullptr, Transform.Matrix());
}

XMMATRIX Camera::ProjectionMatrix() const
{
	return XMLoadFloat4x4(&m_Projection);
}

DirectX::XMMATRIX Camera::InverseViewMatrix() const
{
	return XMMatrixInverse(nullptr, ViewMatrix());
}

XMMATRIX Camera::InverseProjectionMatrix() const
{
	return XMMatrixInverse(nullptr, ProjectionMatrix());
}

XMMATRIX Camera::ViewProjectionMatrix() const
{
	return XMMatrixMultiply(ViewMatrix(), ProjectionMatrix());
}

XMMATRIX Camera::InverseViewProjectionMatrix() const
{
	return XMMatrixInverse(nullptr, ViewProjectionMatrix());
}

void Camera::SetLookAt(DirectX::XMVECTOR EyePosition, DirectX::XMVECTOR FocusPosition, DirectX::XMVECTOR UpDirection)
{
	XMMATRIX view = XMMatrixLookAtLH(EyePosition, FocusPosition, UpDirection);
	Transform.SetTransform(XMMatrixInverse(nullptr, view));
}

void Camera::SetProjection(DirectX::XMMATRIX M)
{
	XMStoreFloat4x4(&m_Projection, M);
}

void Camera::SetPosition(float x, float y, float z)
{
	Transform.Position = XMFLOAT3(x, y, z);
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

OrthographicCamera::OrthographicCamera()
{
	m_ViewLeft = m_ViewRight = m_ViewBottom = m_ViewTop = 0.0f;
}

OrthographicCamera::OrthographicCamera(float NearZ, float FarZ)
	: Camera(NearZ, FarZ)
{
	m_ViewLeft = m_ViewRight = m_ViewBottom = m_ViewTop = 0.0f;
}

float OrthographicCamera::ViewLeft() const
{
	return m_ViewLeft;
}

float OrthographicCamera::ViewRight() const
{
	return m_ViewRight;
}

float OrthographicCamera::ViewBottom() const
{
	return m_ViewBottom;
}

float OrthographicCamera::ViewTop() const
{
	return m_ViewTop;
}

void OrthographicCamera::SetLens(float ViewLeft, float ViewRight, float ViewBottom, float ViewTop, float NearZ, float FarZ)
{
	m_ViewLeft = ViewLeft;
	m_ViewRight = ViewRight;
	m_ViewBottom = ViewBottom;
	m_ViewTop = ViewTop;
	m_NearZ = NearZ;
	m_FarZ = FarZ;

	UpdateProjectionMatrix();
}

void OrthographicCamera::UpdateProjectionMatrix()
{
	XMMATRIX projection = XMMatrixOrthographicOffCenterLH(m_ViewLeft, m_ViewRight, m_ViewBottom, m_ViewTop, m_NearZ, m_FarZ);
	XMStoreFloat4x4(&m_Projection, projection);
}

PerspectiveCamera::PerspectiveCamera()
{
	m_FoVY = m_AspectRatio = 0.0f;
	Aperture = 0.0f;
	FocalLength = 10.0f;
}

PerspectiveCamera::PerspectiveCamera(float NearZ, float FarZ)
	: Camera(NearZ, FarZ)
{
	m_FoVY = m_AspectRatio = 0.0f;
}

float PerspectiveCamera::FoVY() const
{
	return m_FoVY;
}

float PerspectiveCamera::AspectRatio() const
{
	return m_AspectRatio;
}

DirectX::XMVECTOR PerspectiveCamera::GetUVector() const
{
	return Transform.Right() * FocalLength * tanf(m_FoVY * 0.5f) * m_AspectRatio;
}

DirectX::XMVECTOR PerspectiveCamera::GetVVector() const
{
	return Transform.Up() * FocalLength * tanf(m_FoVY * 0.5f);
}

DirectX::XMVECTOR PerspectiveCamera::GetWVector() const
{
	return Transform.Forward() * FocalLength;
}

void PerspectiveCamera::SetFoVY(float FoVY)
{
	SetLens(FoVY, m_AspectRatio, m_NearZ, m_FarZ);
}

void PerspectiveCamera::SetAspectRatio(float AspectRatio)
{
	SetLens(m_FoVY, AspectRatio, m_NearZ, m_FarZ);
}

void PerspectiveCamera::SetNearZ(float NearZ)
{
	SetLens(m_FoVY, m_AspectRatio, NearZ, m_FarZ);
}

void PerspectiveCamera::SetFarZ(float FarZ)
{
	SetLens(m_FoVY, m_AspectRatio, m_NearZ, FarZ);
}

void PerspectiveCamera::SetLens(float FoVY, float AspectRatio, float NearZ, float FarZ)
{
	m_FoVY = FoVY;
	m_AspectRatio = AspectRatio;
	m_NearZ = NearZ;
	m_FarZ = FarZ;

	UpdateProjectionMatrix();
}

void PerspectiveCamera::UpdateProjectionMatrix()
{
	XMMATRIX projection = XMMatrixPerspectiveFovLH(m_FoVY, m_AspectRatio, m_NearZ, m_FarZ);
	XMStoreFloat4x4(&m_Projection, projection);
}