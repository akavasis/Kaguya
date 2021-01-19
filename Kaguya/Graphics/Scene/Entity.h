#pragma once
#include <entt.hpp>
#include "Scene.h"

struct Entity
{
	Entity() = default;
	Entity(entt::entity Handle, Scene* pScene);

	template<typename T, typename... Args>
	T& AddComponent(Args&&... args)
	{
		T& component = pScene->m_Registry.emplace<T>(Handle, std::forward<Args>(args)...);
		pScene->OnComponentAdded<T>(*this, component);
		return component;
	}

	template<typename T>
	T& GetComponent()
	{
		return pScene->m_Registry.get<T>(Handle);
	}

	template<typename T>
	bool HasComponent()
	{
		return pScene->m_Registry.has<T>(Handle);
	}

	template<typename T>
	void RemoveComponent()
	{
		pScene->m_Registry.remove<T>(Handle);
	}

	operator bool() const { return Handle != entt::null; }
	operator entt::entity() const { return Handle; }
	operator uint32_t() const { return (uint32_t)Handle; }

	bool operator==(const Entity& other) const
	{
		return Handle == other.Handle && pScene == other.pScene;
	}

	bool operator!=(const Entity& other) const
	{
		return !(*this == other);
	}

	entt::entity Handle = entt::null;
	Scene* pScene = nullptr;
};