#pragma once
#include "Math/Transform.h"

class Camera
{
public:
	Camera();
	Camera(float NearZ, float FarZ);

	auto operator<=>(const Camera&) const = default;

	inline float NearZ() const { return m_NearZ; }
	inline float FarZ() const { return m_FarZ; }
	DirectX::XMMATRIX ViewMatrix() const;
	DirectX::XMMATRIX ProjectionMatrix() const;
	DirectX::XMMATRIX ViewProjectionMatrix() const;
	DirectX::XMMATRIX InverseViewMatrix() const;
	DirectX::XMMATRIX InverseProjectionMatrix() const;
	DirectX::XMMATRIX InverseViewProjectionMatrix() const;

	void SetLookAt(DirectX::XMVECTOR EyePosition, DirectX::XMVECTOR FocusPosition, DirectX::XMVECTOR UpDirection);

	void Translate(float DeltaX, float DeltaY, float DeltaZ);
	void Rotate(float AngleX, float AngleY);

	Transform Transform;
protected:
	virtual DirectX::XMMATRIX GetProjectionMatrix() const = 0;

	float m_NearZ;
	float m_FarZ;
};

class OrthographicCamera : public Camera
{
public:
	OrthographicCamera();
	OrthographicCamera(float NearZ, float FarZ);

	auto operator<=>(const OrthographicCamera&) const = default;

	inline float ViewLeft() const { return m_ViewLeft; }
	inline float ViewRight() const { return m_ViewRight; }
	inline float ViewBottom() const { return m_ViewBottom; }
	inline float ViewTop() const { return m_ViewTop; }

	void SetLens(float ViewLeft, float ViewRight, float ViewBottom, float ViewTop, float NearZ, float FarZ);
private:
	DirectX::XMMATRIX GetProjectionMatrix() const override;

	float m_ViewLeft;
	float m_ViewRight;
	float m_ViewBottom;
	float m_ViewTop;
};

using FStop = float;
using ISO = float;
using EV = float;

class PerspectiveCamera : public Camera
{
public:
	PerspectiveCamera();
	PerspectiveCamera(float NearZ, float FarZ);

	auto operator<=>(const PerspectiveCamera&) const = default;

	float FoVY() const;
	float AspectRatio() const;
	DirectX::XMVECTOR GetUVector() const;
	DirectX::XMVECTOR GetVVector() const;
	DirectX::XMVECTOR GetWVector() const;
	EV ExposureValue100() const;

	// Radians
	void SetFoVY(float FoVY);
	void SetAspectRatio(float AspectRatio);
	void SetNearZ(float NearZ);
	void SetFarZ(float FarZ);
	void SetLens(float FoVY, float AspectRatio, float NearZ, float FarZ);

	float	FocalLength;		// controls the focus distance of the camera. Impacts the depth of field.
	FStop	RelativeAperture;	// controls how wide the aperture is opened. Impacts the depth of field.
	float	ShutterTime;		// controls how long the aperture is opened. Impacts the motion blur.
	ISO		SensorSensitivity;	// controls how photons are counted/quantized on the digital sensor.
private:
	DirectX::XMMATRIX GetProjectionMatrix() const override;

	float m_FoVY;
	float m_AspectRatio;
};