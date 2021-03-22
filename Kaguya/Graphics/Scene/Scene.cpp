#include "pch.h"
#include "Scene.h"

#include "Entity.h"

#include "../AssetManager.h"

Scene::Scene()
{
	Camera.Transform.Position = { 0.0f, 5.0f, -10.0f };
	PreviousCamera = Camera;
}

void Scene::Clear()
{
	SceneState = SCENE_STATE_RENDER;
	Registry.clear();
	Camera.Transform.Position = { 0.0f, 2.0f, -10.0f };
	PreviousCamera = Camera;
}

void Scene::Update()
{
	SceneState = SCENE_STATE_RENDER;

	// Update all transform
	{
		auto view = Registry.view<Transform>();
		for (auto [handle, transform] : view.each())
		{
			if (transform.IsEdited)
			{
				transform.IsEdited = false;

				SceneState = SCENE_STATE_UPDATED;
			}
		}
	}

	// Update all mesh filters
	{
		auto view = Registry.view<MeshFilter>();
		for (auto [handle, meshFilter] : view.each())
		{
			if (meshFilter.IsEdited)
			{
				meshFilter.IsEdited = false;

				SceneState = SCENE_STATE_UPDATED;
			}

			meshFilter.Mesh = AssetManager::Instance().GetMeshCache().Load(meshFilter.MeshID);
		}
	}

	// Update all mesh renderers
	{
		auto view = Registry.view<MeshFilter, MeshRenderer>();
		for (auto [handle, meshFilter, meshRenderer] : view.each())
		{
			// I have to manually update the mesh filters here because I keep on getting
			// segfault when I add new mesh filter to entt, need to investigate
			meshRenderer.pMeshFilter = &meshFilter;

			if (meshRenderer.IsEdited)
			{
				meshRenderer.IsEdited = false;

				SceneState = SCENE_STATE_UPDATED;
			}
		}
	}

	// Update all lights
	{
		auto view = Registry.view<Light>();
		for (auto [handle, light] : view.each())
		{
			if (light.IsEdited)
			{
				light.IsEdited = false;

				SceneState = SCENE_STATE_UPDATED;
			}
		}
	}

	if (PreviousCamera != Camera)
	{
		SceneState = SCENE_STATE_UPDATED;
	}
}

Entity Scene::CreateEntity(const std::string& Name)
{
	Entity NewEntity = { Registry.create(), this };
	auto& TagComponent = NewEntity.AddComponent<Tag>();
	TagComponent.Name = Name;

	NewEntity.AddComponent<Transform>();

	return NewEntity;
}

void Scene::DestroyEntity(Entity Entity)
{
	Registry.destroy(Entity);
}

template<typename T>
void Scene::OnComponentAdded(Entity Entity, T& Component)
{

}

template<>
void Scene::OnComponentAdded<Tag>(Entity Entity, Tag& Component)
{

}

template<>
void Scene::OnComponentAdded<Transform>(Entity Entity, Transform& Component)
{

}

template<>
void Scene::OnComponentAdded<MeshFilter>(Entity Entity, MeshFilter& Component)
{
	// If we added a MeshFilter component, check to see if we have a MeshRenderer component
	// and connect them
	if (Entity.HasComponent<MeshRenderer>())
	{
		auto& MeshRendererComponent = Entity.GetComponent<MeshRenderer>();
		MeshRendererComponent.pMeshFilter = &Component;

		MeshRendererComponent.IsEdited = true;
	}
}

template<>
void Scene::OnComponentAdded<MeshRenderer>(Entity Entity, MeshRenderer& Component)
{
	// If we added a MeshRenderer component, check to see if we have a MeshFilter component
	// and connect them
	if (Entity.HasComponent<MeshFilter>())
	{
		auto& MeshFilterComponent = Entity.GetComponent<MeshFilter>();
		Component.pMeshFilter = &MeshFilterComponent;

		Component.IsEdited = true;
	}
}

template<>
void Scene::OnComponentAdded<Light>(Entity Entity, Light& Component)
{

}