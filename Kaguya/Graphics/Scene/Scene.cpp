#include "pch.h"
#include "Scene.h"

#include "Entity.h"

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
	}
}