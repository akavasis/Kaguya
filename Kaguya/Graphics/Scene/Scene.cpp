#include "pch.h"
#include "Scene.h"
using namespace DirectX;

PolygonalLight& Scene::AddLight()
{
	std::string Name = "Light [" + std::to_string(Lights.size()) + "]";

	auto& light = Lights.emplace_back(Name.data());
	return light;
}

Material& Scene::AddMaterial(Material&& Material)
{
	std::string Name = "Material [" + std::to_string(Materials.size()) + "]";

	auto& material = Materials.emplace_back(std::move(Material));
	material.Name = std::move(Name);
	return material;
}

Model& Scene::AddModel(Model&& Model)
{
	Models.push_back(std::move(Model));
	return Models.back();
}

ModelInstance& Scene::AddModelInstance(ModelInstance&& ModelInstance)
{
	ModelInstances.push_back(std::move(ModelInstance));
	return ModelInstances.back();
}

uint32_t CullModels(const Camera* pCamera, const Scene::ModelInstanceList& ModelInstances, std::vector<const ModelInstance*>& Indices)
{
	DirectX::BoundingFrustum frustum(pCamera->ProjectionMatrix());
	frustum.Transform(frustum, pCamera->Transform.Matrix());

	UINT numVisible = 0;
	for (auto iter = ModelInstances.begin(); iter != ModelInstances.end(); ++iter)
	{
		auto& model = (*iter);
		DirectX::BoundingBox aabb;
		model.BoundingBox.Transform(aabb, model.Transform.Matrix());
		if (frustum.Contains(aabb) != DirectX::ContainmentType::DISJOINT)
		{
			Indices[numVisible++] = &(*iter);
		}
	}

	return numVisible;
}

uint32_t CullModelsOrthographic(const OrthographicCamera& Camera, bool IgnoreNearZ, const Scene::ModelInstanceList& ModelInstances, std::vector<const ModelInstance*>& Indices)
{
	XMVECTOR mins = XMVectorSet(Camera.ViewLeft(), Camera.ViewBottom(), Camera.NearZ(), 1.0f);
	XMVECTOR maxes = XMVectorSet(Camera.ViewRight(), Camera.ViewTop(), Camera.FarZ(), 1.0f);
	if (IgnoreNearZ)
	{
		mins = XMVectorSet(XMVectorGetX(mins), XMVectorGetY(mins), XMVectorGetX(g_XMNegInfinity.v), 1.0f);
	}

	XMVECTOR extents = (maxes - mins) * 0.5f;
	XMVECTOR center = mins + extents;
	center = XMVector3TransformCoord(center, XMMatrixRotationQuaternion(XMLoadFloat4(&Camera.Transform.Orientation)));
	center += XMLoadFloat3(&Camera.Transform.Position);

	BoundingOrientedBox obb;
	XMStoreFloat3(&obb.Extents, extents);
	XMStoreFloat3(&obb.Center, center);
	obb.Orientation = Camera.Transform.Orientation;

	UINT numVisible = 0;
	for (auto iter = ModelInstances.begin(); iter != ModelInstances.end(); ++iter)
	{
		auto& model = (*iter);
		BoundingBox aabb;
		model.BoundingBox.Transform(aabb, model.Transform.Matrix());
		if (obb.Contains(aabb) != ContainmentType::DISJOINT)
		{
			Indices[numVisible++] = &(*iter);
		}
	}

	return numVisible;
}

uint32_t CullMeshes(const Camera* pCamera, const std::vector<MeshInstance>& MeshInstances, std::vector<uint32_t>& Indices)
{
	BoundingFrustum frustum(pCamera->ProjectionMatrix());
	frustum.Transform(frustum, pCamera->Transform.Matrix());

	UINT numVisible = 0;
	for (size_t i = 0, numMeshes = MeshInstances.size(); i < numMeshes; ++i)
	{
		auto& mesh = MeshInstances[i];
		BoundingBox aabb;
		mesh.BoundingBox.Transform(aabb, mesh.Transform.Matrix());
		if (frustum.Contains(aabb) != ContainmentType::DISJOINT)
		{
			Indices[numVisible++] = i;
		}
	}

	return numVisible;
}

uint32_t CullMeshesOrthographic(const OrthographicCamera& Camera, bool IgnoreNearZ, const std::vector<MeshInstance>& MeshInstances, std::vector<uint32_t>& Indices)
{
	XMVECTOR mins = XMVectorSet(Camera.ViewLeft(), Camera.ViewBottom(), Camera.NearZ(), 1.0f);
	XMVECTOR maxes = XMVectorSet(Camera.ViewRight(), Camera.ViewTop(), Camera.FarZ(), 1.0f);
	if (IgnoreNearZ)
	{
		mins = XMVectorSet(XMVectorGetX(mins), XMVectorGetY(mins), XMVectorGetX(g_XMNegInfinity.v), 1.0f);
	}

	XMVECTOR extents = (maxes - mins) / 2.0f;
	XMVECTOR center = mins + extents;
	center = XMVector3TransformCoord(center, XMMatrixRotationQuaternion(XMLoadFloat4(&Camera.Transform.Orientation)));
	center += XMLoadFloat3(&Camera.Transform.Position);

	BoundingOrientedBox obb;
	XMStoreFloat3(&obb.Extents, extents);
	XMStoreFloat3(&obb.Center, center);
	obb.Orientation = Camera.Transform.Orientation;

	UINT numVisible = 0;
	for (size_t i = 0, numMeshes = MeshInstances.size(); i < numMeshes; ++i)
	{
		auto& mesh = MeshInstances[i];
		BoundingBox aabb;
		mesh.BoundingBox.Transform(aabb, mesh.Transform.Matrix());
		if (obb.Contains(aabb) != ContainmentType::DISJOINT)
		{
			Indices[numVisible++] = i;
		}
	}

	return numVisible;
}