#pragma once
#include <entt.hpp>

#include "Components/Tag.h"
#include "Components/Transform.h"
#include "Components/MeshFilter.h"
#include "Components/MeshRenderer.h"

#include "../Asset/Mesh.h"

#include "../Camera.h"
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
	static constexpr UINT64 MAX_INSTANCE_SUPPORTED = 1000;

	Scene();

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
		.Albedo = Material.Albedo,
		.Emissive = Material.Emissive,
		.Specular = Material.Specular,
		.Refraction = Material.Refraction,
		.SpecularChance = Material.SpecularChance,
		.Roughness = Material.Roughness,
		.Metallic = Material.Metallic,
		.Fuzziness = Material.Fuzziness,
		.IndexOfRefraction = Material.IndexOfRefraction,
		.Model = (uint)Material.Model,
		.UseAttributeAsValues = (uint)Material.UseAttributes, // If this is true, then the attributes above will be used rather than actual textures
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

inline HLSL::Camera GetHLSLCameraDesc(const Camera& Camera)
{
	DirectX::XMFLOAT4 Position = { Camera.Transform.Position.x, Camera.Transform.Position.y, Camera.Transform.Position.z, 1.0f };
	DirectX::XMFLOAT4 U, V, W;
	DirectX::XMFLOAT4X4 View, Projection, ViewProjection, InvView, InvProjection, InvViewProjection;

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