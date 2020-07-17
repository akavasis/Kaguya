#include "pch.h"
#include "Transform.h"

Transform::Transform()
{
	Reset();
}

void Transform::Reset()
{
	DirectX::XMStoreFloat3(&Position, DirectX::XMVectorSet(0.0f, 0.0f, 0.0f, 1.0f));
	DirectX::XMStoreFloat4(&Orientation, DirectX::XMQuaternionIdentity());
	DirectX::XMStoreFloat3(&Scale, DirectX::XMVectorSet(1.0f, 1.0f, 1.0f, 0.0f));
}

void Transform::SetTransform(DirectX::FXMMATRIX M)
{
	DirectX::XMVECTOR translation, orientation, scale;
	DirectX::XMMatrixDecompose(&scale, &orientation, &translation, M);

	DirectX::XMStoreFloat3(&Position, translation);
	DirectX::XMStoreFloat4(&Orientation, orientation);
	DirectX::XMStoreFloat3(&Scale, scale);
}

void Transform::Translate(float DeltaX, float DeltaY, float DeltaZ)
{
	DirectX::XMVECTOR CurrentPosition = XMLoadFloat3(&Position);
	DirectX::XMVECTOR Velocity = DirectX::XMVector3Rotate(DirectX::XMVectorSet(DeltaX, DeltaY, DeltaZ, 0.0f), DirectX::XMLoadFloat4(&Orientation));
	DirectX::XMStoreFloat3(&Position, DirectX::XMVectorAdd(CurrentPosition, Velocity));
}

void Transform::SetScale(float ScaleX, float ScaleY, float ScaleZ)
{
	Scale.x = ScaleX;
	Scale.y = ScaleY;
	Scale.z = ScaleZ;
}

void Transform::Rotate(float AngleX, float AngleY, float AngleZ)
{
	DirectX::XMVECTOR currentRotation = DirectX::XMLoadFloat4(&Orientation);
	DirectX::XMVECTOR pitch = DirectX::XMQuaternionNormalize(DirectX::XMQuaternionRotationAxis(Right(), AngleX));
	DirectX::XMVECTOR yaw = DirectX::XMQuaternionNormalize(DirectX::XMQuaternionRotationAxis(Math::Up, AngleY));
	DirectX::XMVECTOR roll = DirectX::XMQuaternionNormalize(DirectX::XMQuaternionRotationAxis(Forward(), AngleZ));

	DirectX::XMStoreFloat4(&Orientation, DirectX::XMQuaternionMultiply(currentRotation, pitch));
	currentRotation = DirectX::XMLoadFloat4(&Orientation);
	DirectX::XMStoreFloat4(&Orientation, DirectX::XMQuaternionMultiply(currentRotation, yaw));
	currentRotation = DirectX::XMLoadFloat4(&Orientation);
	DirectX::XMStoreFloat4(&Orientation, DirectX::XMQuaternionMultiply(currentRotation, roll));
}

DirectX::XMMATRIX Transform::Matrix() const
{
	// S * R * T
	return
		DirectX::XMMatrixMultiply(
			DirectX::XMMatrixMultiply(DirectX::XMMatrixScalingFromVector(DirectX::XMLoadFloat3(&Scale)), DirectX::XMMatrixRotationQuaternion(DirectX::XMQuaternionNormalize(XMLoadFloat4(&Orientation)))),
			DirectX::XMMatrixTranslationFromVector(DirectX::XMLoadFloat3(&Position)));
}

DirectX::XMVECTOR Transform::Up() const
{
	return DirectX::XMVector3Rotate(Math::Up, DirectX::XMLoadFloat4(&Orientation));
}

DirectX::XMVECTOR Transform::Right() const
{
	return DirectX::XMVector3Rotate(Math::Right, DirectX::XMLoadFloat4(&Orientation));
}

DirectX::XMVECTOR Transform::Forward() const
{
	return DirectX::XMVector3Rotate(Math::Forward, DirectX::XMLoadFloat4(&Orientation));
}