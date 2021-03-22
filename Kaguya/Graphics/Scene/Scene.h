#pragma once
#include <entt.hpp>

#include "Components/Tag.h"
#include "Components/Transform.h"
#include "Components/MeshFilter.h"
#include "Components/MeshRenderer.h"
#include "Components/Light.h"

#include "Camera.h"

#include "../SharedTypes.h"

struct Entity;

struct Scene
{
	enum State
	{
		SCENE_STATE_RENDER,
		SCENE_STATE_UPDATED,
	};

	static constexpr UINT64 MAX_MATERIAL_SUPPORTED = 1000;
	static constexpr UINT64 MAX_LIGHT_SUPPORTED = 100;
	static constexpr UINT64 MAX_INSTANCE_SUPPORTED = 1000;

	Scene();

	void Clear();

	void Update();

	Entity CreateEntity(const std::string& Name);
	void DestroyEntity(Entity Entity);

	template<typename T>
	void OnComponentAdded(Entity entity, T& component);

	State SceneState = SCENE_STATE_RENDER;
	entt::registry Registry;

	Camera Camera, PreviousCamera;
};

inline HLSL::Material GetHLSLMaterialDesc(const Material& Material)
{
	return
	{
		.baseColor = Material.baseColor,
		.metallic = Material.metallic,
		.subsurface = Material.subsurface,
		.specular = Material.specular,
		.roughness = Material.roughness,
		.specularTint = Material.specularTint,
		.anisotropic = Material.anisotropic,
		.sheen = Material.sheen,
		.sheenTint = Material.sheenTint,
		.clearcoat = Material.clearcoat,
		.clearcoatGloss = Material.clearcoatGloss,
		.TextureIndices =
		{
			Material.TextureIndices[0],
			Material.TextureIndices[1],
			Material.TextureIndices[2],
			Material.TextureIndices[3],
			Material.TextureIndices[4]
		},
		.TextureChannel =
		{
			Material.TextureChannel[0],
			Material.TextureChannel[1],
			Material.TextureChannel[2],
			Material.TextureChannel[3],
			Material.TextureChannel[4],
		}
	};
}

inline HLSL::Light GetHLSLLightDesc(const Transform& Transform, const Light& Light)
{
	using namespace DirectX;

	XMMATRIX M = Transform.Matrix();

	float4 Orientation; XMStoreFloat4(&Orientation, DirectX::XMVector3Normalize(Transform.Forward()));
	float3 Points[4] = {};
	float halfWidth = Light.Width * 0.5f;
	float halfHeight = Light.Height * 0.5f;
	// Get billboard points at the origin
	XMVECTOR p0 = XMVectorSet(+halfWidth, -halfHeight, 0, 1);
	XMVECTOR p1 = XMVectorSet(+halfWidth, +halfHeight, 0, 1);
	XMVECTOR p2 = XMVectorSet(-halfWidth, +halfHeight, 0, 1);
	XMVECTOR p3 = XMVectorSet(-halfWidth, -halfHeight, 0, 1);

	// Precompute the light points here so ray generation shader doesnt have to do it for every ray
	// Move points to light's location
	float3 points[4];
	XMStoreFloat3(&points[0], XMVector3TransformCoord(p0, M));
	XMStoreFloat3(&points[1], XMVector3TransformCoord(p1, M));
	XMStoreFloat3(&points[2], XMVector3TransformCoord(p2, M));
	XMStoreFloat3(&points[3], XMVector3TransformCoord(p3, M));

	return
	{
		.Type = (uint)Light.Type,
		.Position = Transform.Position,
		.Orientation = Orientation,
		.Width = Light.Width,
		.Height = Light.Height,
		.Points =
		{
			points[0],
			points[1],
			points[2],
			points[3],
		},

		.I = Light.I
	};
}

inline HLSL::Camera GetHLSLCameraDesc(const Camera& Camera)
{
	using namespace DirectX;

	XMFLOAT4 Position = { Camera.Transform.Position.x, Camera.Transform.Position.y, Camera.Transform.Position.z, 1.0f };
	XMFLOAT4 U, V, W;
	XMFLOAT4X4 View, Projection, ViewProjection, InvView, InvProjection, InvViewProjection;

	XMStoreFloat4(&U, Camera.GetUVector());
	XMStoreFloat4(&V, Camera.GetVVector());
	XMStoreFloat4(&W, Camera.GetWVector());

	XMStoreFloat4x4(&View, XMMatrixTranspose(Camera.ViewMatrix()));
	XMStoreFloat4x4(&Projection, XMMatrixTranspose(Camera.ProjectionMatrix()));
	XMStoreFloat4x4(&ViewProjection, XMMatrixTranspose(Camera.ViewProjectionMatrix()));
	XMStoreFloat4x4(&InvView, XMMatrixTranspose(Camera.InverseViewMatrix()));
	XMStoreFloat4x4(&InvProjection, XMMatrixTranspose(Camera.InverseProjectionMatrix()));
	XMStoreFloat4x4(&InvViewProjection, XMMatrixTranspose(Camera.InverseViewProjectionMatrix()));

	return
	{
		.NearZ = Camera.NearZ,
		.FarZ = Camera.FarZ,
		.ExposureValue100 = Camera.ExposureValue100(),
		._padding1 = 0.0f,

		.FocalLength = Camera.FocalLength,
		.RelativeAperture = Camera.RelativeAperture,
		.ShutterTime = Camera.ShutterTime,
		.SensorSensitivity = Camera.SensorSensitivity,

		.Position = Position,
		.U = U,
		.V = V,
		.W = W,

		.View = View,
		.Projection = Projection,
		.ViewProjection = ViewProjection,
		.InvView = InvView,
		.InvProjection = InvProjection,
		.InvViewProjection = InvViewProjection
	};
}