#pragma once
#include <entt.hpp>

#include "Components/Tag.h"
#include "Components/Transform.h"
#include "Components/MeshFilter.h"
#include "Components/MeshRenderer.h"

struct Entity;

struct Scene
{
	static constexpr UINT64 MAX_LIGHT_SUPPORTED = 1000;
	static constexpr UINT64 MAX_MATERIAL_SUPPORTED = 1000;
	static constexpr UINT64 MAX_MESH_INSTANCE_SUPPORTED = 1000;

	auto& operator->()
	{
		return Registry;
	}

	auto& operator->() const
	{
		return Registry;
	}

	Entity CreateEntity(const std::string& Name);
	void DestroyEntity(Entity Entity);

	template<typename T>
	void OnComponentAdded(Entity entity, T& component);

	entt::registry Registry;
};