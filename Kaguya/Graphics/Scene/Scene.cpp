#include "pch.h"
#include "Scene.h"

#include "Entity.h"

Entity Scene::CreateEntity(const std::string& Name)
{
	Entity NewEntity = { m_Registry.create(), this };
	auto& TagComponent = NewEntity.AddComponent<Tag>();
	TagComponent.Name = Name;

	NewEntity.AddComponent<Transform>();

	return NewEntity;
}

void Scene::DestroyEntity(Entity Entity)
{
	m_Registry.destroy(Entity);
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