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
		T& Component = pScene->Registry.emplace<T>(Handle, std::forward<Args>(args)...);
		Component.Handle = Handle;
		Component.pScene = pScene;
		Component.IsEdited = true;
		pScene->OnComponentAdded<T>(*this, Component);
		return Component;
	}

	template<typename T>
	T& GetComponent()
	{
		return pScene->Registry.get<T>(Handle);
	}

	template<typename T, typename... Args>
	T& GetOrAddComponent(Args&&... args)
	{
		if (HasComponent<T>())
		{
			return GetComponent<T>();
		}
		return AddComponent<T>(args...);
	}

	template<typename T>
	bool HasComponent()
	{
		return pScene->Registry.has<T>(Handle);
	}

	template<typename T>
	void RemoveComponent()
	{
		pScene->Registry.remove<T>(Handle);
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