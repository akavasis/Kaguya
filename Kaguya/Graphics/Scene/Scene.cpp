#include "pch.h"
#include "Scene.h"

PointLight& Scene::AddPointLight(PointLight&& PointLight)
{
	PointLights.push_back(std::move(PointLight));
	return PointLights.back();
}

SpotLight& Scene::AddSpotLight(SpotLight&& SpotLight)
{
	SpotLights.push_back(std::move(SpotLight));
	return SpotLights.back();
}

Material& Scene::AddMaterial(Material&& Material)
{
	Materials.push_back(std::move(Material));
	return Materials.back();
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
	using namespace DirectX;

	DirectX::XMVECTOR mins = DirectX::XMVectorSet(Camera.ViewLeft(), Camera.ViewBottom(), Camera.NearZ(), 1.0f);
	DirectX::XMVECTOR maxes = DirectX::XMVectorSet(Camera.ViewRight(), Camera.ViewTop(), Camera.FarZ(), 1.0f);
	if (IgnoreNearZ)
	{
		mins = XMVectorSet(XMVectorGetX(mins), XMVectorGetY(mins), XMVectorGetX(g_XMNegInfinity.v), 1.0f);
	}

	DirectX::XMVECTOR extents = (maxes - mins) * 0.5f;
	DirectX::XMVECTOR center = mins + extents;
	center = XMVector3TransformCoord(center, XMMatrixRotationQuaternion(XMLoadFloat4(&Camera.Transform.Orientation)));
	center += XMLoadFloat3(&Camera.Transform.Position);

	DirectX::BoundingOrientedBox obb;
	XMStoreFloat3(&obb.Extents, extents);
	XMStoreFloat3(&obb.Center, center);
	obb.Orientation = Camera.Transform.Orientation;

	UINT numVisible = 0;
	for (auto iter = ModelInstances.begin(); iter != ModelInstances.end(); ++iter)
	{
		auto& model = (*iter);
		DirectX::BoundingBox aabb;
		model.BoundingBox.Transform(aabb, model.Transform.Matrix());
		if (obb.Contains(aabb) != DirectX::ContainmentType::DISJOINT)
		{
			Indices[numVisible++] = &(*iter);
		}
	}

	return numVisible;
}

uint32_t CullMeshes(const Camera* pCamera, const std::vector<MeshInstance>& MeshInstances, std::vector<uint32_t>& Indices)
{
	DirectX::BoundingFrustum frustum(pCamera->ProjectionMatrix());
	frustum.Transform(frustum, pCamera->Transform.Matrix());

	UINT numVisible = 0;
	for (size_t i = 0, numMeshes = MeshInstances.size(); i < numMeshes; ++i)
	{
		auto& mesh = MeshInstances[i];
		DirectX::BoundingBox aabb;
		mesh.BoundingBox.Transform(aabb, mesh.Transform.Matrix());
		if (frustum.Contains(aabb) != DirectX::ContainmentType::DISJOINT)
		{
			Indices[numVisible++] = i;
		}
	}

	return numVisible;
}

uint32_t CullMeshesOrthographic(const OrthographicCamera& Camera, bool IgnoreNearZ, const std::vector<MeshInstance>& MeshInstances, std::vector<uint32_t>& Indices)
{
	using namespace DirectX;

	DirectX::XMVECTOR mins = DirectX::XMVectorSet(Camera.ViewLeft(), Camera.ViewBottom(), Camera.NearZ(), 1.0f);
	DirectX::XMVECTOR maxes = DirectX::XMVectorSet(Camera.ViewRight(), Camera.ViewTop(), Camera.FarZ(), 1.0f);
	if (IgnoreNearZ)
	{
		mins = XMVectorSet(XMVectorGetX(mins), XMVectorGetY(mins), XMVectorGetX(g_XMNegInfinity.v), 1.0f);
	}

	DirectX::XMVECTOR extents = (maxes - mins) / 2.0f;
	DirectX::XMVECTOR center = mins + extents;
	center = XMVector3TransformCoord(center, XMMatrixRotationQuaternion(XMLoadFloat4(&Camera.Transform.Orientation)));
	center += XMLoadFloat3(&Camera.Transform.Position);

	DirectX::BoundingOrientedBox obb;
	XMStoreFloat3(&obb.Extents, extents);
	XMStoreFloat3(&obb.Center, center);
	obb.Orientation = Camera.Transform.Orientation;

	UINT numVisible = 0;
	for (size_t i = 0, numMeshes = MeshInstances.size(); i < numMeshes; ++i)
	{
		auto& mesh = MeshInstances[i];
		DirectX::BoundingBox aabb;
		mesh.BoundingBox.Transform(aabb, mesh.Transform.Matrix());
		if (obb.Contains(aabb) != DirectX::ContainmentType::DISJOINT)
		{
			Indices[numVisible++] = i;
		}
	}

	return numVisible;
}