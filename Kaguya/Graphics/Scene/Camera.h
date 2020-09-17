#pragma once
#include "Math/Transform.h"

class Camera
{
public:
	Camera();
	Camera(float NearZ, float FarZ);

	inline float NearZ() const { return m_NearZ; }
	inline float FarZ() const { return m_FarZ; }
	DirectX::XMMATRIX ViewMatrix() const;
	DirectX::XMMATRIX ProjectionMatrix() const;
	DirectX::XMMATRIX InverseViewMatrix() const;
	DirectX::XMMATRIX InverseProjectionMatrix() const;
	DirectX::XMMATRIX ViewProjectionMatrix() const;
	DirectX::XMMATRIX InverseViewProjectionMatrix() const;

	void SetLookAt(DirectX::XMVECTOR EyePosition, DirectX::XMVECTOR FocusPosition, DirectX::XMVECTOR UpDirection);
	void SetProjection(DirectX::XMMATRIX M);

	void SetPosition(float x, float y, float z);

	void Translate(float DeltaX, float DeltaY, float DeltaZ);
	void Rotate(float AngleX, float AngleY);

	Transform Transform;
protected:
	static constexpr float s_TranslateSpeed = 15.0f;
	static constexpr float s_RotationSpeed = 15.0f;

	DirectX::XMFLOAT4X4 m_Projection;
	float m_NearZ;
	float m_FarZ;

	virtual void UpdateProjectionMatrix() = 0;
};

class OrthographicCamera : public Camera
{
public:
	OrthographicCamera();
	OrthographicCamera(float NearZ, float FarZ);

	float ViewLeft() const;
	float ViewRight() const;
	float ViewBottom() const;
	float ViewTop() const;

	void SetLens(float ViewLeft, float ViewRight, float ViewBottom, float ViewTop, float NearZ, float FarZ);
protected:
	void UpdateProjectionMatrix() override;
private:
	float m_ViewLeft;
	float m_ViewRight;
	float m_ViewBottom;
	float m_ViewTop;
};

class PerspectiveCamera : public Camera
{
public:
	PerspectiveCamera();
	PerspectiveCamera(float NearZ, float FarZ);

	float FoVY() const;
	float AspectRatio() const;
	DirectX::XMVECTOR GetUVector() const;
	DirectX::XMVECTOR GetVVector() const;
	DirectX::XMVECTOR GetWVector() const;

	// Radians
	void SetFoVY(float FoVY);
	void SetAspectRatio(float AspectRatio);
	void SetNearZ(float NearZ);
	void SetFarZ(float FarZ);
	void SetLens(float FoVY, float AspectRatio, float NearZ, float FarZ);

	float Aperture;
	float FocalLength;
protected:
	void UpdateProjectionMatrix() override;
private:
	float m_FoVY;
	float m_AspectRatio;
};