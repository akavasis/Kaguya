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

Model& Scene::AddModel(Model&& Model)
{
	Models.push_back(std::move(Model));
	return Models.back();
}

void Scene::RenderImGuiWindow()
{
	if (ImGui::Begin("Scene"))
	{
		Sun.RenderImGuiWindow();
		for (auto& PointLight : PointLights)
		{
			PointLight.RenderImGuiWindow();
		}
		for (auto& SpotLight : SpotLights)
		{
			SpotLight.RenderImGuiWindow();
		}
	}
	ImGui::End();
}

UINT CullModels(const Camera* pCamera, const Scene::ModelList& Models, std::vector<const Model*>& Indices)
{
	DirectX::BoundingFrustum frustum(pCamera->ProjectionMatrix());
	frustum.Transform(frustum, pCamera->WorldMatrix());

	UINT numVisible = 0;
	for (auto iter = Models.begin(); iter != Models.end(); ++iter)
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

UINT CullModelsOrthographic(const OrthographicCamera& Camera, bool IgnoreNearZ, const Scene::ModelList& Models, std::vector<const Model*>& Indices)
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
	center = XMVector3TransformCoord(center, XMMatrixRotationQuaternion(XMLoadFloat4(&Camera.GetTransform().Orientation)));
	center += XMLoadFloat3(&Camera.GetTransform().Position);

	DirectX::BoundingOrientedBox obb;
	XMStoreFloat3(&obb.Extents, extents);
	XMStoreFloat3(&obb.Center, center);
	obb.Orientation = Camera.GetTransform().Orientation;

	UINT numVisible = 0;
	for (auto iter = Models.begin(); iter != Models.end(); ++iter)
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

UINT CullMeshes(const Camera* pCamera, const std::vector<Mesh>& Meshes, std::vector<UINT>& Indices)
{
	DirectX::BoundingFrustum frustum(pCamera->ProjectionMatrix());
	frustum.Transform(frustum, pCamera->WorldMatrix());

	UINT numVisible = 0;
	for (size_t i = 0, numMeshes = Meshes.size(); i < numMeshes; ++i)
	{
		auto& mesh = Meshes[i];
		DirectX::BoundingBox aabb;
		mesh.BoundingBox.Transform(aabb, mesh.Transform.Matrix());
		if (frustum.Contains(aabb) != DirectX::ContainmentType::DISJOINT)
		{
			Indices[numVisible++] = i;
		}
	}

	return numVisible;
}

UINT CullMeshesOrthographic(const OrthographicCamera& Camera, bool IgnoreNearZ, const std::vector<Mesh>& Meshes, std::vector<UINT>& Indices)
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
	center = XMVector3TransformCoord(center, XMMatrixRotationQuaternion(XMLoadFloat4(&Camera.GetTransform().Orientation)));
	center += XMLoadFloat3(&Camera.GetTransform().Position);

	DirectX::BoundingOrientedBox obb;
	XMStoreFloat3(&obb.Extents, extents);
	XMStoreFloat3(&obb.Center, center);
	obb.Orientation = Camera.GetTransform().Orientation;

	UINT numVisible = 0;
	for (size_t i = 0, numMeshes = Meshes.size(); i < numMeshes; ++i)
	{
		auto& mesh = Meshes[i];
		DirectX::BoundingBox aabb;
		mesh.BoundingBox.Transform(aabb, mesh.Transform.Matrix());
		if (obb.Contains(aabb) != DirectX::ContainmentType::DISJOINT)
		{
			Indices[numVisible++] = i;
		}
	}

	return numVisible;
}